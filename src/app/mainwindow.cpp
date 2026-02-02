#include "mainwindow.h"

#include <KActionCollection>
#include <KLocalizedString>
#include <KRecentFilesAction>
#include <KStandardAction>
#include <KConfigGroup>
#include <KSharedConfig>

#include "pagelayout.h"
#include "pagelayoutwidget.h"
#include "sidebar.h"
#include "toolview.h"
#include "codeblockhighlighter.h"
#include "documentbuilder.h"
#include "documenttab.h"
#include "documentview.h"
#include "filebrowserdock.h"
#include "metadatastore.h"
#include "tocwidget.h"
#include "printcontroller.h"
#include "stylemanager.h"
#include "styledockwidget.h"
#include "thememanager.h"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QFile>
#include <QFileDialog>
#include <QLabel>
#include <QMenuBar>
#include <QAbstractTextDocumentLayout>
#include <QSplitter>
#include <QSpinBox>
#include <QStatusBar>
#include <QTabWidget>
#include <QTextBlock>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent)
    : KXmlGuiWindow(parent)
{
    setAttribute(Qt::WA_DeleteOnClose, false);

    // Central widget: splitter with sidebars + tabbed document area
    m_splitter = new QSplitter(Qt::Horizontal, this);

    m_tabWidget = new QTabWidget;
    m_tabWidget->setTabsClosable(true);
    m_tabWidget->setMovable(true);
    m_tabWidget->setDocumentMode(true);

    connect(m_tabWidget, &QTabWidget::tabCloseRequested,
            m_tabWidget, &QTabWidget::removeTab);
    connect(m_tabWidget, &QTabWidget::currentChanged,
            this, [this]() {
        auto *view = currentDocumentView();
        if (view && m_zoomSpinBox) {
            const QSignalBlocker blocker(m_zoomSpinBox);
            m_zoomSpinBox->setValue(view->zoomPercent());
        }
    });

    m_themeManager = new ThemeManager(this);
    m_metadataStore = new MetadataStore(this);

    setupSidebars();

    // Assemble splitter: left sidebar | tabs | right sidebar
    m_splitter->addWidget(m_leftSidebar);
    m_splitter->addWidget(m_tabWidget);
    m_splitter->addWidget(m_rightSidebar);

    // Set stretch factors: sidebars don't stretch, center does
    m_splitter->setStretchFactor(0, 0);
    m_splitter->setStretchFactor(1, 1);
    m_splitter->setStretchFactor(2, 0);

    // Prevent user from collapsing widgets to zero via handle drag
    m_splitter->setCollapsible(0, false);
    m_splitter->setCollapsible(1, false);
    m_splitter->setCollapsible(2, false);

    // Sidebars start collapsed — lock them to tab bar width
    m_leftSidebar->setCollapsed(true);
    m_rightSidebar->setCollapsed(true);

    setCentralWidget(m_splitter);

    setupActions();

    // Status bar with zoom indicator
    m_zoomSpinBox = new QSpinBox;
    m_zoomSpinBox->setRange(25, 400);
    m_zoomSpinBox->setSuffix(QStringLiteral("%"));
    m_zoomSpinBox->setValue(100);
    m_zoomSpinBox->setFixedWidth(80);
    m_zoomSpinBox->setToolTip(i18n("Zoom level"));
    statusBar()->addPermanentWidget(m_zoomSpinBox);
    connect(m_zoomSpinBox, qOverload<int>(&QSpinBox::valueChanged),
            this, [this](int value) {
        auto *view = currentDocumentView();
        if (view)
            view->setZoomPercent(value);
    });
    statusBar()->showMessage(i18n("Ready"));

    setMinimumSize(800, 600);
    resize(1200, 800);

    // Load the first theme so the style tree is populated on startup
    QStringList themes = m_themeManager->availableThemes();
    if (!themes.isEmpty()) {
        onThemeChanged(themes.first());
    }

    restoreSession();
}

MainWindow::~MainWindow() = default;

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveSession();

    KConfigGroup recentGroup(KSharedConfig::openConfig(),
                             QStringLiteral("RecentFiles"));
    if (m_recentFilesAction)
        m_recentFilesAction->saveEntries(recentGroup);

    KXmlGuiWindow::closeEvent(event);
    qApp->quit();
}

void MainWindow::setupSidebars()
{
    // Left sidebar: File Browser + TOC
    m_leftSidebar = new Sidebar(Sidebar::Left, this);

    m_fileBrowserWidget = new FileBrowserDock(this);
    auto *filesView = new ToolView(i18n("Files"), m_fileBrowserWidget);
    m_filesBrowserTabId = m_leftSidebar->addPanel(
        filesView, QIcon::fromTheme(QStringLiteral("folder")), i18n("Files"));

    connect(m_fileBrowserWidget, &FileBrowserDock::fileActivated,
            this, &MainWindow::openFile);

    m_tocWidget = new TocWidget(this);
    auto *tocView = new ToolView(i18n("Contents"), m_tocWidget);
    m_tocTabId = m_leftSidebar->addPanel(
        tocView, QIcon::fromTheme(QStringLiteral("format-list-ordered")), i18n("Contents"));

    connect(m_tocWidget, &TocWidget::headingClicked,
            this, [this](int blockNumber) {
        auto *view = currentDocumentView();
        if (!view || !view->document())
            return;
        QTextBlock block = view->document()->findBlockByNumber(blockNumber);
        if (block.isValid()) {
            QSizeF pageSize = view->document()->pageSize();
            if (pageSize.height() > 0) {
                auto *layout = view->document()->documentLayout();
                QRectF blockRect = layout->blockBoundingRect(block);
                int page = static_cast<int>(blockRect.top() / pageSize.height());
                view->goToPage(page);
            }
        }
    });

    // Right sidebar: Style panel + Page Layout panel
    m_rightSidebar = new Sidebar(Sidebar::Right, this);

    m_styleDockWidget = new StyleDockWidget(m_themeManager, this);
    auto *styleView = new ToolView(i18n("Style"), m_styleDockWidget);
    m_styleTabId = m_rightSidebar->addPanel(
        styleView, QIcon::fromTheme(QStringLiteral("preferences-desktop-font")), i18n("Style"));

    m_pageLayoutWidget = new PageLayoutWidget(this);
    auto *pageView = new ToolView(i18n("Page"), m_pageLayoutWidget);
    m_pageLayoutTabId = m_rightSidebar->addPanel(
        pageView, QIcon::fromTheme(QStringLiteral("document-properties")), i18n("Page"));

    m_styleDockWidget->setPageLayoutProvider([this]() {
        return m_pageLayoutWidget->currentPageLayout();
    });
    connect(m_styleDockWidget, &StyleDockWidget::themeChanged,
            this, &MainWindow::onThemeChanged);
    connect(m_styleDockWidget, &StyleDockWidget::styleOverrideChanged,
            this, &MainWindow::onStyleOverrideChanged);
    connect(m_pageLayoutWidget, &PageLayoutWidget::pageLayoutChanged,
            this, &MainWindow::onPageLayoutChanged);
}

void MainWindow::setupActions()
{
    KActionCollection *ac = actionCollection();

    // Standard actions
    KStandardAction::quit(qApp, &QApplication::quit, ac);
    KStandardAction::preferences(this, []() {}, ac); // placeholder

    // File > Open
    auto *openAction = KStandardAction::open(this, &MainWindow::onFileOpen, ac);
    Q_UNUSED(openAction);

    // File > Open Recent
    m_recentFilesAction = KStandardAction::openRecent(
        this, &MainWindow::onFileOpenRecent, ac);
    KConfigGroup recentGroup(KSharedConfig::openConfig(),
                             QStringLiteral("RecentFiles"));
    m_recentFilesAction->loadEntries(recentGroup);

    // File > Export PDF
    auto *exportPdf = ac->addAction(QStringLiteral("file_export_pdf"));
    exportPdf->setText(i18n("Export as &PDF..."));
    exportPdf->setIcon(QIcon::fromTheme(QStringLiteral("document-export")));
    connect(exportPdf, &QAction::triggered, this, &MainWindow::onFileExportPdf);

    // File > Print
    auto *printAction = KStandardAction::print(this, &MainWindow::onFilePrint, ac);
    Q_UNUSED(printAction);

    // File > Close
    auto *closeAction = KStandardAction::close(this, &MainWindow::onFileClose, ac);
    Q_UNUSED(closeAction);

    // View > Zoom
    KStandardAction::zoomIn(this, &MainWindow::onZoomIn, ac);
    KStandardAction::zoomOut(this, &MainWindow::onZoomOut, ac);

    auto *fitWidth = ac->addAction(QStringLiteral("view_zoom_fit_width"));
    fitWidth->setText(i18n("Fit &Width"));
    fitWidth->setIcon(QIcon::fromTheme(QStringLiteral("zoom-fit-width")));
    connect(fitWidth, &QAction::triggered, this, &MainWindow::onFitWidth);

    auto *fitPage = ac->addAction(QStringLiteral("view_zoom_fit_page"));
    fitPage->setText(i18n("Fit &Page"));
    fitPage->setIcon(QIcon::fromTheme(QStringLiteral("zoom-fit-page")));
    connect(fitPage, &QAction::triggered, this, &MainWindow::onFitPage);

    // View > Mode
    auto *continuous = ac->addAction(QStringLiteral("view_continuous"));
    continuous->setText(i18n("&Continuous Scroll"));
    continuous->setCheckable(true);
    continuous->setChecked(true);
    connect(continuous, &QAction::triggered, this, [this]() {
        auto *view = currentDocumentView();
        if (view)
            view->setContinuousMode(true);
    });

    auto *singlePage = ac->addAction(QStringLiteral("view_single_page"));
    singlePage->setText(i18n("&Single Page"));
    singlePage->setCheckable(true);
    connect(singlePage, &QAction::triggered, this, [this]() {
        auto *view = currentDocumentView();
        if (view)
            view->setContinuousMode(false);
    });

    // Go > Navigation
    auto *prevPage = ac->addAction(QStringLiteral("go_previous_page"));
    prevPage->setText(i18n("&Previous Page"));
    prevPage->setIcon(QIcon::fromTheme(QStringLiteral("go-previous")));
    ac->setDefaultShortcut(prevPage, QKeySequence(Qt::Key_PageUp));
    connect(prevPage, &QAction::triggered, this, [this]() {
        auto *view = currentDocumentView();
        if (view)
            view->previousPage();
    });

    auto *nextPage = ac->addAction(QStringLiteral("go_next_page"));
    nextPage->setText(i18n("&Next Page"));
    nextPage->setIcon(QIcon::fromTheme(QStringLiteral("go-next")));
    ac->setDefaultShortcut(nextPage, QKeySequence(Qt::Key_PageDown));
    connect(nextPage, &QAction::triggered, this, [this]() {
        auto *view = currentDocumentView();
        if (view)
            view->nextPage();
    });

    // View > Source Mode
    auto *sourceMode = ac->addAction(QStringLiteral("view_source_mode"));
    sourceMode->setText(i18n("&Source Mode"));
    sourceMode->setIcon(QIcon::fromTheme(QStringLiteral("text-x-script")));
    sourceMode->setCheckable(true);
    ac->setDefaultShortcut(sourceMode, QKeySequence(Qt::CTRL | Qt::Key_U));
    connect(sourceMode, &QAction::triggered,
            this, &MainWindow::onToggleSourceMode);

    // Sidebar toggle actions
    auto *toggleFiles = ac->addAction(QStringLiteral("view_toggle_files"));
    toggleFiles->setText(i18n("&Files Panel"));
    toggleFiles->setIcon(QIcon::fromTheme(QStringLiteral("folder")));
    toggleFiles->setCheckable(true);
    connect(toggleFiles, &QAction::triggered, this, [this](bool checked) {
        if (checked)
            m_leftSidebar->showPanel(m_filesBrowserTabId);
        else
            m_leftSidebar->hidePanel(m_filesBrowserTabId);
    });
    connect(m_leftSidebar, &Sidebar::panelVisibilityChanged,
            this, [this, toggleFiles](int tabId, bool visible) {
        if (tabId == m_filesBrowserTabId)
            toggleFiles->setChecked(visible);
    });

    auto *toggleToc = ac->addAction(QStringLiteral("view_toggle_toc"));
    toggleToc->setText(i18n("&Contents Panel"));
    toggleToc->setIcon(QIcon::fromTheme(QStringLiteral("format-list-ordered")));
    toggleToc->setCheckable(true);
    connect(toggleToc, &QAction::triggered, this, [this](bool checked) {
        if (checked)
            m_leftSidebar->showPanel(m_tocTabId);
        else
            m_leftSidebar->hidePanel(m_tocTabId);
    });
    connect(m_leftSidebar, &Sidebar::panelVisibilityChanged,
            this, [this, toggleToc](int tabId, bool visible) {
        if (tabId == m_tocTabId)
            toggleToc->setChecked(visible);
    });

    auto *toggleStyle = ac->addAction(QStringLiteral("view_toggle_style"));
    toggleStyle->setText(i18n("&Style Panel"));
    toggleStyle->setIcon(QIcon::fromTheme(QStringLiteral("preferences-desktop-font")));
    toggleStyle->setCheckable(true);
    connect(toggleStyle, &QAction::triggered, this, [this](bool checked) {
        if (checked)
            m_rightSidebar->showPanel(m_styleTabId);
        else
            m_rightSidebar->hidePanel(m_styleTabId);
    });
    connect(m_rightSidebar, &Sidebar::panelVisibilityChanged,
            this, [this, toggleStyle](int tabId, bool visible) {
        if (tabId == m_styleTabId)
            toggleStyle->setChecked(visible);
    });

    setupGUI(Default, QStringLiteral("prettyreaderui.rc"));
}

void MainWindow::onFileOpen()
{
    const QUrl url = QFileDialog::getOpenFileUrl(
        this,
        i18n("Open Markdown File"),
        QUrl::fromLocalFile(QDir::homePath()),
        i18n("Markdown Files (*.md *.markdown *.mkd *.txt);;All Files (*)"));

    if (url.isValid()) {
        openFile(url);
    }
}

void MainWindow::onFileOpenRecent(const QUrl &url)
{
    openFile(url);
}

void MainWindow::onFileExportPdf()
{
    auto *view = currentDocumentView();
    if (!view || !view->document()) {
        statusBar()->showMessage(i18n("No document to export."), 3000);
        return;
    }

    auto *controller = new PrintController(view->document(), this);
    PageLayout pl = m_pageLayoutWidget->currentPageLayout();
    controller->setPageLayout(pl);
    QString title = m_tabWidget->tabText(m_tabWidget->currentIndex());
    controller->setFileName(title);
    controller->exportPdf(QString(), this);
    delete controller;
}

void MainWindow::onFilePrint()
{
    auto *view = currentDocumentView();
    if (!view || !view->document()) {
        statusBar()->showMessage(i18n("No document to print."), 3000);
        return;
    }

    auto *controller = new PrintController(view->document(), this);
    PageLayout pl = m_pageLayoutWidget->currentPageLayout();
    controller->setPageLayout(pl);
    QString title = m_tabWidget->tabText(m_tabWidget->currentIndex());
    controller->setFileName(title);
    controller->print(this);
    delete controller;
}

void MainWindow::onFileClose()
{
    int index = m_tabWidget->currentIndex();
    if (index >= 0) {
        m_tabWidget->removeTab(index);
    }
}

void MainWindow::onThemeChanged(const QString &themeId)
{
    // Load theme into a new StyleManager and hand it to the dock
    auto *sm = new StyleManager(this);
    if (!m_themeManager->loadTheme(themeId, sm))
        m_themeManager->loadDefaults(sm);
    m_styleDockWidget->populateFromStyleManager(sm);
    delete sm;

    // Re-apply page layout from theme
    PageLayout pl = m_themeManager->themePageLayout();
    auto *view = currentDocumentView();
    if (view)
        view->setPageLayout(pl);

    rebuildCurrentDocument();
}

void MainWindow::onStyleOverrideChanged()
{
    rebuildCurrentDocument();
}

void MainWindow::onPageLayoutChanged()
{
    PageLayout pl = m_pageLayoutWidget->currentPageLayout();
    auto *view = currentDocumentView();
    if (view)
        view->setPageLayout(pl);
    rebuildCurrentDocument();
}

void MainWindow::onZoomIn()
{
    auto *view = currentDocumentView();
    if (view)
        view->zoomIn();
}

void MainWindow::onZoomOut()
{
    auto *view = currentDocumentView();
    if (view)
        view->zoomOut();
}

void MainWindow::onFitWidth()
{
    auto *view = currentDocumentView();
    if (view)
        view->fitWidth();
}

void MainWindow::onFitPage()
{
    auto *view = currentDocumentView();
    if (view)
        view->fitPage();
}

void MainWindow::onToggleSourceMode()
{
    auto *tab = currentDocumentTab();
    if (!tab)
        return;

    bool enteringSource = !tab->isSourceMode();
    tab->setSourceMode(enteringSource);

    if (!enteringSource) {
        // Switching back to reader mode - rebuild from source text
        rebuildCurrentDocument();
    }

    statusBar()->showMessage(
        enteringSource ? i18n("Source mode") : i18n("Reader mode"), 2000);
}

DocumentView *MainWindow::currentDocumentView() const
{
    auto *tab = currentDocumentTab();
    return tab ? tab->documentView() : nullptr;
}

DocumentTab *MainWindow::currentDocumentTab() const
{
    int index = m_tabWidget->currentIndex();
    if (index < 0)
        return nullptr;
    return qobject_cast<DocumentTab *>(m_tabWidget->widget(index));
}

void MainWindow::saveSession()
{
    KConfigGroup group(KSharedConfig::openConfig(),
                       QStringLiteral("Session"));
    QStringList files;
    for (int i = 0; i < m_tabWidget->count(); ++i) {
        QString path = m_tabWidget->tabToolTip(i);
        if (!path.isEmpty())
            files.append(path);
    }
    group.writeEntry("OpenFiles", files);
    group.writeEntry("ActiveTab", m_tabWidget->currentIndex());

    // Save sidebar state
    group.writeEntry("LeftSidebarCollapsed", m_leftSidebar->isCollapsed());
    group.writeEntry("RightSidebarCollapsed", m_rightSidebar->isCollapsed());
    if (m_splitter)
        group.writeEntry("SplitterSizes", m_splitter->sizes());
    group.sync();
}

void MainWindow::restoreSession()
{
    KConfigGroup group(KSharedConfig::openConfig(),
                       QStringLiteral("Session"));
    QStringList files = group.readEntry("OpenFiles", QStringList());
    int activeTab = group.readEntry("ActiveTab", 0);

    for (const QString &path : files) {
        if (QFile::exists(path))
            openFile(QUrl::fromLocalFile(path));
    }

    if (activeTab >= 0 && activeTab < m_tabWidget->count())
        m_tabWidget->setCurrentIndex(activeTab);

    // Restore sidebar state
    bool leftCollapsed = group.readEntry("LeftSidebarCollapsed", true);
    bool rightCollapsed = group.readEntry("RightSidebarCollapsed", true);

    // Expand sidebars that were open last session (unlocks width constraints)
    if (!leftCollapsed)
        m_leftSidebar->showPanel(m_filesBrowserTabId);
    if (!rightCollapsed)
        m_rightSidebar->showPanel(m_styleTabId);

    // Restore splitter proportions from last session
    QList<int> splitterSizes = group.readEntry("SplitterSizes", QList<int>());
    if (splitterSizes.size() == 3 && m_splitter) {
        // Validate: expanded sidebars need meaningful width
        if (!leftCollapsed && splitterSizes[0] < 100)
            splitterSizes[0] = 250;
        if (!rightCollapsed && splitterSizes[2] < 100)
            splitterSizes[2] = 250;
        m_splitter->setSizes(splitterSizes);
    }
}

void MainWindow::rebuildCurrentDocument()
{
    auto *tab = currentDocumentTab();
    if (!tab)
        return;

    QString filePath = tab->filePath();
    if (filePath.isEmpty())
        return;

    // Use source text from editor if in source mode, otherwise read from file
    QString markdown;
    if (tab->isSourceMode()) {
        markdown = tab->sourceText();
    } else {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            return;
        markdown = QString::fromUtf8(file.readAll());
        file.close();
    }

    // Use the editing copy from the style dock, or load fresh if none
    StyleManager *editingSm = m_styleDockWidget->currentStyleManager();
    StyleManager *styleManager;
    if (editingSm) {
        styleManager = editingSm->clone(this);
    } else {
        styleManager = new StyleManager(this);
        QString themeId = m_styleDockWidget->currentThemeId();
        if (!m_themeManager->loadTheme(themeId, styleManager))
            m_themeManager->loadDefaults(styleManager);
    }

    auto *doc = new QTextDocument(this);
    auto *builder = new DocumentBuilder(doc, this);
    builder->setBasePath(QFileInfo(filePath).absolutePath());
    builder->setStyleManager(styleManager);
    builder->build(markdown);

    CodeBlockHighlighter rebuildHighlighter;
    rebuildHighlighter.highlight(doc);

    // Replace document in current tab's DocumentView, preserving view state
    auto *view = tab->documentView();
    if (view) {
        ViewState state = view->saveViewState();
        QTextDocument *oldDoc = view->document();
        view->setDocument(doc);
        delete oldDoc;
        view->restoreViewState(state);

        QFileInfo fi(filePath);
        view->setDocumentInfo(fi.fileName(), fi.baseName());
    }

    // Update TOC
    m_tocWidget->buildFromDocument(doc);

    statusBar()->showMessage(i18n("Theme applied"), 2000);
}

void MainWindow::openFile(const QUrl &url)
{
    if (!url.isLocalFile())
        return;

    const QString filePath = url.toLocalFile();

    // Check if already open
    for (int i = 0; i < m_tabWidget->count(); ++i) {
        if (m_tabWidget->tabToolTip(i) == filePath) {
            m_tabWidget->setCurrentIndex(i);
            return;
        }
    }

    // Read file
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        statusBar()->showMessage(
            i18n("Failed to open %1", filePath), 5000);
        return;
    }
    const QString markdown = QString::fromUtf8(file.readAll());
    file.close();

    // Build document with style manager (use editing copy if available)
    StyleManager *editingSm = m_styleDockWidget->currentStyleManager();
    StyleManager *styleManager;
    if (editingSm) {
        styleManager = editingSm->clone(this);
    } else {
        styleManager = new StyleManager(this);
        QString themeId = m_styleDockWidget->currentThemeId();
        if (!m_themeManager->loadTheme(themeId, styleManager))
            m_themeManager->loadDefaults(styleManager);
    }

    auto *doc = new QTextDocument(this);
    auto *builder = new DocumentBuilder(doc, this);
    builder->setBasePath(QFileInfo(filePath).absolutePath());
    builder->setStyleManager(styleManager);
    builder->build(markdown);

    CodeBlockHighlighter highlighter;
    highlighter.highlight(doc);

    // Display in DocumentTab (reader + source modes)
    auto *tab = new DocumentTab(this);
    tab->setFilePath(filePath);
    tab->setSourceText(markdown);
    PageLayout openPl = m_pageLayoutWidget->currentPageLayout();
    tab->documentView()->setPageLayout(openPl);
    tab->documentView()->setDocument(doc);

    QFileInfo fi(filePath);
    tab->documentView()->setDocumentInfo(fi.fileName(), fi.baseName());

    // Connect zoom signal to status bar spinbox
    connect(tab->documentView(), &DocumentView::zoomChanged,
            this, [this](int percent) {
        if (m_zoomSpinBox) {
            const QSignalBlocker blocker(m_zoomSpinBox);
            m_zoomSpinBox->setValue(percent);
        }
    });
    int index = m_tabWidget->addTab(tab, fi.fileName());
    m_tabWidget->setTabToolTip(index, filePath);
    m_tabWidget->setCurrentIndex(index);

    m_recentFilesAction->addUrl(url);

    // Update TOC
    m_tocWidget->buildFromDocument(doc);

    statusBar()->showMessage(i18n("Opened %1", fi.fileName()), 3000);
}
