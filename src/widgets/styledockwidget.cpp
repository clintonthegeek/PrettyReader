#include "styledockwidget.h"
#include "footnoteconfigwidget.h"
#include "stylepropertieseditor.h"
#include "tablestylepropertieseditor.h"
#include "styletreemodel.h"
#include "characterstyle.h"
#include "paragraphstyle.h"
#include "stylemanager.h"
#include "tablestyle.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QScrollArea>
#include <QStackedWidget>
#include <QTreeView>
#include <QVBoxLayout>

StyleDockWidget::StyleDockWidget(QWidget *parent)
    : QWidget(parent)
{
    buildUI();
}

void StyleDockWidget::buildUI()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

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

StyleManager *StyleDockWidget::currentStyleManager() const
{
    return m_editingStyles;
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
}

void StyleDockWidget::refreshTreeModel()
{
    if (!m_editingStyles)
        return;
    m_treeModel->setStyleManager(m_editingStyles);
    if (m_showPreviewsCheck->isChecked())
        m_treeModel->refresh();
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
