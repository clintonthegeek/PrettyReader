#include "styledockwidget.h"
#include "stylepropertieseditor.h"
#include "styletreemodel.h"
#include "characterstyle.h"
#include "pagelayout.h"
#include "paragraphstyle.h"
#include "stylemanager.h"
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
    auto *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    auto *content = new QWidget;
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    // --- Theme section ---
    auto *themeLabel = new QLabel(tr("Theme"));
    themeLabel->setStyleSheet(QStringLiteral("font-weight: bold;"));
    layout->addWidget(themeLabel);

    // Theme combo + buttons
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

    // --- Style Tree ---
    auto *stylesLabel = new QLabel(tr("Styles"));
    stylesLabel->setStyleSheet(QStringLiteral("font-weight: bold;"));
    layout->addWidget(stylesLabel);

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
    m_styleTree->setMaximumHeight(250);
    layout->addWidget(m_styleTree);

    connect(m_styleTree->selectionModel(), &QItemSelectionModel::currentChanged,
            this, [this]() { onTreeSelectionChanged(); });

    // --- Properties Editor ---
    m_propsEditor = new StylePropertiesEditor;
    layout->addWidget(m_propsEditor);

    connect(m_propsEditor, &StylePropertiesEditor::propertyChanged,
            this, &StyleDockWidget::onStylePropertyChanged);

    scrollArea->setWidget(content);

    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->addWidget(scrollArea);
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

    // Clear properties editor until a style is selected
    m_propsEditor->clear();
    m_selectedStyleName.clear();

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
        m_selectedStyleName.clear();
        return;
    }

    m_selectedStyleName = m_treeModel->styleName(current);
    m_selectedIsParagraph = m_treeModel->isParagraphStyle(current);
    loadSelectedStyle();
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
