#ifndef PRETTYREADER_STYLEDOCKWIDGET_H
#define PRETTYREADER_STYLEDOCKWIDGET_H

#include <QWidget>

class QCheckBox;
class QStackedWidget;
class QTreeView;
class FootnoteConfigWidget;
class StyleManager;
class StylePropertiesEditor;
class StyleTreeModel;
class TableStylePropertiesEditor;

class StyleDockWidget : public QWidget
{
    Q_OBJECT

public:
    explicit StyleDockWidget(QWidget *parent = nullptr);

    // Get the editing copy of styles (replaces old applyOverrides)
    StyleManager *currentStyleManager() const;

    // Populate the dock from a new theme's StyleManager
    void populateFromStyleManager(StyleManager *sm);

    // Refresh the style tree model after external compose changes
    void refreshTreeModel();

signals:
    void styleOverrideChanged();

private slots:
    void onStylePropertyChanged();
    void onTableStylePropertyChanged();
    void onFootnoteStyleChanged();
    void onTreeSelectionChanged();

private:
    void buildUI();
    void loadSelectedStyle();
    void loadSelectedTableStyle();

    StyleManager *m_editingStyles = nullptr;

    // Style tree + editor
    QCheckBox *m_showPreviewsCheck = nullptr;
    QTreeView *m_styleTree = nullptr;
    StyleTreeModel *m_treeModel = nullptr;
    QStackedWidget *m_editorStack = nullptr;
    StylePropertiesEditor *m_propsEditor = nullptr;
    TableStylePropertiesEditor *m_tablePropsEditor = nullptr;

    FootnoteConfigWidget *m_footnoteConfig = nullptr;

    QString m_selectedStyleName;
    bool m_selectedIsParagraph = true;
    bool m_selectedIsTable = false;
    bool m_selectedIsFootnote = false;
};

#endif // PRETTYREADER_STYLEDOCKWIDGET_H
