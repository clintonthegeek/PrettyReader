#ifndef PRETTYREADER_MAINWINDOW_H
#define PRETTYREADER_MAINWINDOW_H

#include <KXmlGuiWindow>

class QCloseEvent;
class QSplitter;
class QSpinBox;
class QTabWidget;
class KRecentFilesAction;
class FileBrowserDock;
class FontManager;
class MetadataStore;
class PageLayoutWidget;
class Sidebar;
class TextShaper;
class TocWidget;
class ThemeManager;
class StyleDockWidget;
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
    void onThemeChanged(const QString &themeId);
    void onStyleOverrideChanged();
    void onPageLayoutChanged();
    void onZoomIn();
    void onZoomOut();
    void onFitWidth();
    void onFitPage();
    void onToggleSourceMode();
    void showPreferences();
    void onSettingsChanged();

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

    StyleDockWidget *m_styleDockWidget = nullptr;
    PageLayoutWidget *m_pageLayoutWidget = nullptr;
    FileBrowserDock *m_fileBrowserWidget = nullptr;
    TocWidget *m_tocWidget = nullptr;
    int m_pageLayoutTabId = -1;
    KRecentFilesAction *m_recentFilesAction = nullptr;
    ThemeManager *m_themeManager = nullptr;
    MetadataStore *m_metadataStore = nullptr;
    Hyphenator *m_hyphenator = nullptr;
    ShortWords *m_shortWords = nullptr;
    QSpinBox *m_zoomSpinBox = nullptr;

    // PDF rendering pipeline (Phase 4)
    FontManager *m_fontManager = nullptr;
    TextShaper *m_textShaper = nullptr;
};

#endif // PRETTYREADER_MAINWINDOW_H
