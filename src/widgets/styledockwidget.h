#ifndef PRETTYREADER_STYLEDOCKWIDGET_H
#define PRETTYREADER_STYLEDOCKWIDGET_H

#include <QWidget>
#include <functional>

class QCheckBox;
class QComboBox;
class QPushButton;
class QTreeView;
class StyleManager;
class StylePropertiesEditor;
class StyleTreeModel;
class ThemeManager;
struct PageLayout;

class StyleDockWidget : public QWidget
{
    Q_OBJECT

public:
    explicit StyleDockWidget(ThemeManager *themeManager,
                             QWidget *parent = nullptr);

    QString currentThemeId() const;
    void setCurrentThemeId(const QString &id);

    // Get the editing copy of styles (replaces old applyOverrides)
    StyleManager *currentStyleManager() const;

    // Populate the dock from a new theme's StyleManager
    void populateFromStyleManager(StyleManager *sm);

    // Provide a callback to get the current page layout for theme saving
    void setPageLayoutProvider(std::function<PageLayout()> provider);

signals:
    void themeChanged(const QString &themeId);
    void styleOverrideChanged();

private slots:
    void onThemeComboChanged(int index);
    void onStylePropertyChanged();
    void onTreeSelectionChanged();
    void onNewTheme();
    void onSaveTheme();
    void onDeleteTheme();
    void onThemesChanged();

private:
    void buildUI();
    void loadSelectedStyle();

    ThemeManager *m_themeManager;
    StyleManager *m_editingStyles = nullptr;
    std::function<PageLayout()> m_pageLayoutProvider;

    // Theme section
    QComboBox *m_themeCombo = nullptr;
    QPushButton *m_newBtn = nullptr;
    QPushButton *m_saveBtn = nullptr;
    QPushButton *m_deleteBtn = nullptr;

    // Style tree + editor
    QCheckBox *m_showPreviewsCheck = nullptr;
    QTreeView *m_styleTree = nullptr;
    StyleTreeModel *m_treeModel = nullptr;
    StylePropertiesEditor *m_propsEditor = nullptr;

    QString m_selectedStyleName;
    bool m_selectedIsParagraph = true;
};

#endif // PRETTYREADER_STYLEDOCKWIDGET_H
