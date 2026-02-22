#ifndef PRETTYREADER_STYLEDOCKWIDGET_H
#define PRETTYREADER_STYLEDOCKWIDGET_H

#include <QWidget>
#include <functional>

class QCheckBox;
class QComboBox;
class QPushButton;
class QStackedWidget;
class QTreeView;
class FontPairingManager;
class FontPairingPickerWidget;
class FootnoteConfigWidget;
class PaletteManager;
class PalettePickerWidget;
class StyleManager;
class StylePropertiesEditor;
class StyleTreeModel;
class TableStylePropertiesEditor;
class ThemeComposer;
class ThemeManager;
struct PageLayout;

class StyleDockWidget : public QWidget
{
    Q_OBJECT

public:
    explicit StyleDockWidget(ThemeManager *themeManager,
                             PaletteManager *paletteManager,
                             FontPairingManager *pairingManager,
                             ThemeComposer *themeComposer,
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
    void onTableStylePropertyChanged();
    void onFootnoteStyleChanged();
    void onTreeSelectionChanged();
    void onNewTheme();
    void onSaveTheme();
    void onDeleteTheme();
    void onThemesChanged();

private:
    void buildUI();
    void loadSelectedStyle();
    void loadSelectedTableStyle();

    ThemeManager *m_themeManager;
    PaletteManager *m_paletteManager;
    FontPairingManager *m_pairingManager;
    ThemeComposer *m_themeComposer;
    StyleManager *m_editingStyles = nullptr;
    std::function<PageLayout()> m_pageLayoutProvider;

    // Theme section
    QComboBox *m_themeCombo = nullptr;
    QPushButton *m_newBtn = nullptr;
    QPushButton *m_saveBtn = nullptr;
    QPushButton *m_deleteBtn = nullptr;

    // Palette & font pairing pickers
    PalettePickerWidget *m_palettePicker = nullptr;
    FontPairingPickerWidget *m_pairingPicker = nullptr;

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
