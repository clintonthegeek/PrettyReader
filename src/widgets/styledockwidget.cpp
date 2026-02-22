#include "styledockwidget.h"
#include "footnoteconfigwidget.h"
#include "stylepropertieseditor.h"
#include "tablestylepropertieseditor.h"
#include "styletreemodel.h"
#include "characterstyle.h"
#include "pagelayout.h"
#include "paragraphstyle.h"
#include "stylemanager.h"
#include "tablestyle.h"
#include "thememanager.h"

#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QTimer>
#include <QTreeView>
#include <QVBoxLayout>

StyleDockWidget::StyleDockWidget(ThemeManager *themeManager, QWidget *parent)
    : QWidget(parent)
    , m_themeManager(themeManager)
{
    buildUI();

    connect(m_themeManager, &ThemeManager::themesChanged,
            this, &StyleDockWidget::onThemesChanged);
}

void StyleDockWidget::buildUI()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    // --- Theme section ---
    auto *themeLabel = new QLabel(tr("Theme"));
    themeLabel->setStyleSheet(QStringLiteral("font-weight: bold;"));
    layout->addWidget(themeLabel);

    auto *themeRow = new QHBoxLayout;
    m_themeCombo = new QComboBox;
    const QStringList themes = m_themeManager->availableThemes();
    for (const QString &id : themes) {
        m_themeCombo->addItem(m_themeManager->themeName(id), id);
    }
    themeRow->addWidget(m_themeCombo, 1);

    m_newBtn = new QPushButton(tr("New"));
    m_newBtn->setFixedWidth(50);
    themeRow->addWidget(m_newBtn);

    m_saveBtn = new QPushButton(tr("Save"));
    m_saveBtn->setFixedWidth(50);
    themeRow->addWidget(m_saveBtn);

    m_deleteBtn = new QPushButton(tr("Del"));
    m_deleteBtn->setFixedWidth(40);
    themeRow->addWidget(m_deleteBtn);

    layout->addLayout(themeRow);

    connect(m_themeCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &StyleDockWidget::onThemeComboChanged);
    connect(m_newBtn, &QPushButton::clicked, this, &StyleDockWidget::onNewTheme);
    connect(m_saveBtn, &QPushButton::clicked, this, &StyleDockWidget::onSaveTheme);
    connect(m_deleteBtn, &QPushButton::clicked, this, &StyleDockWidget::onDeleteTheme);

    // --- Style Tree (stretches to fill available space) ---
    m_showPreviewsCheck = new QCheckBox(tr("Show previews"));
    layout->addWidget(m_showPreviewsCheck);
    connect(m_showPreviewsCheck, &QCheckBox::toggled, this, [this](bool checked) {
        m_treeModel->setShowPreviews(checked);
    });

    m_treeModel = new StyleTreeModel(this);
    m_styleTree = new QTreeView;
    m_styleTree->setModel(m_treeModel);
    m_styleTree->setHeaderHidden(true);
    m_styleTree->setRootIsDecorated(true);
    m_styleTree->setExpandsOnDoubleClick(true);
    m_styleTree->setMinimumHeight(120);
    layout->addWidget(m_styleTree, 1);  // stretch factor 1

    connect(m_styleTree->selectionModel(), &QItemSelectionModel::currentChanged,
            this, [this]() { onTreeSelectionChanged(); });

    // --- Properties Editor (stacked: paragraph/char, table, footnotes) ---
    // Each page wrapped in a scroll area so tall editors scroll independently.
    auto wrapInScroll = [](QWidget *w) -> QScrollArea * {
        auto *sa = new QScrollArea;
        sa->setWidgetResizable(true);
        sa->setFrameShape(QFrame::NoFrame);
        sa->setWidget(w);
        return sa;
    };

    m_propsEditor = new StylePropertiesEditor;
    m_tablePropsEditor = new TableStylePropertiesEditor;
    m_footnoteConfig = new FootnoteConfigWidget;

    m_editorStack = new QStackedWidget;
    m_editorStack->addWidget(wrapInScroll(m_propsEditor));      // index 0
    m_editorStack->addWidget(wrapInScroll(m_tablePropsEditor));  // index 1
    m_editorStack->addWidget(wrapInScroll(m_footnoteConfig));    // index 2
    m_editorStack->hide();
    layout->addWidget(m_editorStack, 1);  // stretch factor 1

    connect(m_propsEditor, &StylePropertiesEditor::propertyChanged,
            this, &StyleDockWidget::onStylePropertyChanged);
    connect(m_tablePropsEditor, &TableStylePropertiesEditor::propertyChanged,
            this, &StyleDockWidget::onTableStylePropertyChanged);
    connect(m_footnoteConfig, &FootnoteConfigWidget::footnoteStyleChanged,
            this, &StyleDockWidget::onFootnoteStyleChanged);
}

QString StyleDockWidget::currentThemeId() const
{
    return m_themeCombo->currentData().toString();
}

void StyleDockWidget::setCurrentThemeId(const QString &id)
{
    for (int i = 0; i < m_themeCombo->count(); ++i) {
        if (m_themeCombo->itemData(i).toString() == id) {
            m_themeCombo->setCurrentIndex(i);
            return;
        }
    }
}

StyleManager *StyleDockWidget::currentStyleManager() const
{
    return m_editingStyles;
}

void StyleDockWidget::setPageLayoutProvider(std::function<PageLayout()> provider)
{
    m_pageLayoutProvider = std::move(provider);
}

void StyleDockWidget::populateFromStyleManager(StyleManager *sm)
{
    // Create a deep copy for editing
    delete m_editingStyles;
    m_editingStyles = sm->clone(const_cast<StyleDockWidget *>(this));

    // Update tree model
    m_treeModel->setStyleManager(m_editingStyles);
    m_styleTree->expandAll();

    // Clear and hide editor until a style is selected
    m_propsEditor->clear();
    m_tablePropsEditor->clear();
    m_selectedStyleName.clear();
    m_selectedIsTable = false;
    m_selectedIsFootnote = false;
    m_editorStack->hide();

    // Pre-load footnote style so it's ready when selected
    m_footnoteConfig->loadFootnoteStyle(m_editingStyles->footnoteStyle());

    // Update delete button state
    m_deleteBtn->setEnabled(!m_themeManager->isBuiltinTheme(currentThemeId()));
}

void StyleDockWidget::onThemeComboChanged(int index)
{
    Q_UNUSED(index);
    Q_EMIT themeChanged(currentThemeId());
}

void StyleDockWidget::onTreeSelectionChanged()
{
    QModelIndex current = m_styleTree->currentIndex();
    if (!current.isValid() || m_treeModel->isCategoryNode(current)) {
        m_propsEditor->clear();
        m_tablePropsEditor->clear();
        m_selectedStyleName.clear();
        m_selectedIsTable = false;
        m_selectedIsFootnote = false;
        m_editorStack->hide();
        return;
    }

    m_selectedIsFootnote = m_treeModel->isFootnoteNode(current);
    if (m_selectedIsFootnote) {
        m_selectedStyleName.clear();
        m_selectedIsTable = false;
        m_editorStack->setCurrentIndex(2);
        m_editorStack->show();
        return;
    }

    m_selectedStyleName = m_treeModel->styleName(current);
    m_selectedIsParagraph = m_treeModel->isParagraphStyle(current);
    m_selectedIsTable = m_treeModel->isTableStyle(current);

    if (m_selectedIsTable) {
        m_editorStack->setCurrentIndex(1);
        loadSelectedTableStyle();
    } else {
        m_editorStack->setCurrentIndex(0);
        loadSelectedStyle();
    }
    m_editorStack->show();
}

void StyleDockWidget::loadSelectedStyle()
{
    if (m_selectedStyleName.isEmpty() || !m_editingStyles)
        return;

    if (m_selectedIsParagraph) {
        ParagraphStyle *style = m_editingStyles->paragraphStyle(m_selectedStyleName);
        if (!style) return;
        ParagraphStyle resolved = m_editingStyles->resolvedParagraphStyle(m_selectedStyleName);
        QStringList parents = m_editingStyles->paragraphStyleNames();
        parents.removeAll(m_selectedStyleName);
        m_propsEditor->loadParagraphStyle(*style, resolved, parents);
    } else {
        CharacterStyle *style = m_editingStyles->characterStyle(m_selectedStyleName);
        if (!style) return;
        CharacterStyle resolved = m_editingStyles->resolvedCharacterStyle(m_selectedStyleName);
        QStringList parents = m_editingStyles->characterStyleNames();
        parents.removeAll(m_selectedStyleName);
        m_propsEditor->loadCharacterStyle(*style, resolved, parents);
    }
}

void StyleDockWidget::onStylePropertyChanged()
{
    if (m_selectedStyleName.isEmpty() || !m_editingStyles)
        return;

    // Create a fresh style with only the explicit properties
    if (m_selectedIsParagraph) {
        ParagraphStyle *existing = m_editingStyles->paragraphStyle(m_selectedStyleName);
        if (!existing) return;
        ParagraphStyle fresh(m_selectedStyleName);
        fresh.setHeadingLevel(existing->headingLevel());
        m_propsEditor->applyToParagraphStyle(fresh);
        m_editingStyles->addParagraphStyle(fresh);
    } else {
        CharacterStyle fresh(m_selectedStyleName);
        m_propsEditor->applyToCharacterStyle(fresh);
        m_editingStyles->addCharacterStyle(fresh);
    }

    // Update tree previews if enabled
    if (m_showPreviewsCheck->isChecked())
        m_treeModel->refresh();

    Q_EMIT styleOverrideChanged();
}

void StyleDockWidget::onNewTheme()
{
    bool ok;
    QString name = QInputDialog::getText(this, tr("New Theme"),
                                          tr("Theme name:"), QLineEdit::Normal,
                                          QString(), &ok);
    if (!ok || name.isEmpty())
        return;

    if (!m_editingStyles)
        return;

    PageLayout pl = m_pageLayoutProvider ? m_pageLayoutProvider() : PageLayout{};
    QString id = m_themeManager->saveTheme(name, m_editingStyles, pl);
    if (!id.isEmpty()) {
        QTimer::singleShot(0, this, [this, id]() {
            setCurrentThemeId(id);
        });
    }
}

void StyleDockWidget::onSaveTheme()
{
    if (!m_editingStyles)
        return;

    QString themeId = currentThemeId();
    PageLayout pl = m_pageLayoutProvider ? m_pageLayoutProvider() : PageLayout{};
    if (m_themeManager->isBuiltinTheme(themeId)) {
        bool ok;
        QString name = QInputDialog::getText(this, tr("Save Theme As"),
                                              tr("New theme name:"), QLineEdit::Normal,
                                              m_themeManager->themeName(themeId) + tr(" (copy)"),
                                              &ok);
        if (!ok || name.isEmpty())
            return;
        QString id = m_themeManager->saveTheme(name, m_editingStyles, pl);
        if (!id.isEmpty()) {
            QTimer::singleShot(0, this, [this, id]() {
                setCurrentThemeId(id);
            });
        }
    } else {
        m_themeManager->saveThemeAs(themeId, m_editingStyles, pl);
    }
}

void StyleDockWidget::onDeleteTheme()
{
    QString themeId = currentThemeId();
    if (m_themeManager->isBuiltinTheme(themeId))
        return;

    int ret = QMessageBox::question(this, tr("Delete Theme"),
                                     tr("Delete theme \"%1\"?")
                                         .arg(m_themeManager->themeName(themeId)));
    if (ret == QMessageBox::Yes) {
        m_themeManager->deleteTheme(themeId);
    }
}

void StyleDockWidget::onThemesChanged()
{
    // Refresh theme combo
    const QSignalBlocker blocker(m_themeCombo);
    QString currentId = currentThemeId();
    m_themeCombo->clear();

    const QStringList themes = m_themeManager->availableThemes();
    for (const QString &id : themes) {
        m_themeCombo->addItem(m_themeManager->themeName(id), id);
    }

    // Restore selection
    setCurrentThemeId(currentId);
    if (m_themeCombo->currentIndex() < 0 && m_themeCombo->count() > 0)
        m_themeCombo->setCurrentIndex(0);

    m_deleteBtn->setEnabled(!m_themeManager->isBuiltinTheme(currentThemeId()));
}

void StyleDockWidget::loadSelectedTableStyle()
{
    if (m_selectedStyleName.isEmpty() || !m_editingStyles)
        return;

    TableStyle *ts = m_editingStyles->tableStyle(m_selectedStyleName);
    if (!ts)
        return;

    QStringList paraNames = m_editingStyles->paragraphStyleNames();
    m_tablePropsEditor->loadTableStyle(*ts, paraNames);
}

void StyleDockWidget::onTableStylePropertyChanged()
{
    if (m_selectedStyleName.isEmpty() || !m_editingStyles)
        return;

    TableStyle fresh(m_selectedStyleName);
    m_tablePropsEditor->applyToTableStyle(fresh);
    m_editingStyles->addTableStyle(fresh);

    Q_EMIT styleOverrideChanged();
}

void StyleDockWidget::onFootnoteStyleChanged()
{
    if (!m_editingStyles)
        return;

    m_editingStyles->setFootnoteStyle(m_footnoteConfig->currentFootnoteStyle());
    Q_EMIT styleOverrideChanged();
}
