#ifndef PRETTYREADER_MAINWINDOW_H
#define PRETTYREADER_MAINWINDOW_H

#include <KXmlGuiWindow>

class QAction;
class QCloseEvent;
class QLabel;
class QSlider;
class QSplitter;
class QSpinBox;
class QTabWidget;
class KActionMenu;
class KRecentFilesAction;
class FileBrowserDock;
class FontManager;
class TypographyThemeManager;
class MetadataStore;
class PageLayoutWidget;
class PaletteManager;
class Sidebar;
class TextShaper;
class ThemeComposer;
class TocWidget;
class ThemeManager;
class StyleDockWidget;
class ThemePickerDock;
class DocumentTab;
class DocumentView;
class Hyphenator;
class ShortWords;

class MainWindow : public KXmlGuiWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    void openFile(const QUrl &url);

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onFileOpen();
    void onFileOpenRecent(const QUrl &url);
    void onFileExportPdf();
    void onFileExportRtf();
    void onFilePrint();
    void onFileClose();
    // onThemeChanged removed — composition is handled by onCompositionApplied()
    void onCompositionApplied();
    void onStyleOverrideChanged();
    void onPageLayoutChanged();
    void onZoomIn();
    void onZoomOut();
    void onFitWidth();
    void onFitPage();
    void onToggleSourceMode();
    void showPreferences();
    void onSettingsChanged();
    void onRenderModeChanged();

private:
    void setupActions();
    void setupSidebars();
    void rebuildCurrentDocument();
    void saveSession();
    void restoreSession();
    DocumentView *currentDocumentView() const;
    DocumentTab *currentDocumentTab() const;

    QSplitter *m_splitter = nullptr;
    QTabWidget *m_tabWidget = nullptr;

    // Sidebars
    Sidebar *m_leftSidebar = nullptr;
    Sidebar *m_rightSidebar = nullptr;
    int m_filesBrowserTabId = -1;
    int m_tocTabId = -1;
    int m_styleTabId = -1;

    ThemePickerDock *m_themePickerDock = nullptr;
    int m_themePickerTabId = -1;
    StyleDockWidget *m_styleDockWidget = nullptr;
    PageLayoutWidget *m_pageLayoutWidget = nullptr;
    FileBrowserDock *m_fileBrowserWidget = nullptr;
    TocWidget *m_tocWidget = nullptr;
    int m_pageLayoutTabId = -1;
    KRecentFilesAction *m_recentFilesAction = nullptr;
    ThemeManager *m_themeManager = nullptr;
    PaletteManager *m_paletteManager = nullptr;
    TypographyThemeManager *m_typographyThemeManager = nullptr;
    ThemeComposer *m_themeComposer = nullptr;
    MetadataStore *m_metadataStore = nullptr;
    Hyphenator *m_hyphenator = nullptr;
    ShortWords *m_shortWords = nullptr;
    QSlider *m_zoomSlider = nullptr;
    QSpinBox *m_zoomSpinBox = nullptr;
    QLabel *m_filePathLabel = nullptr;

    // PDF rendering pipeline (Phase 4)
    FontManager *m_fontManager = nullptr;
    TextShaper *m_textShaper = nullptr;

    // Render mode (Print vs Web)
    QAction *m_webViewAction = nullptr;
    QAction *m_printViewAction = nullptr;
    KActionMenu *m_pageArrangementMenu = nullptr;
};

#endif // PRETTYREADER_MAINWINDOW_H
