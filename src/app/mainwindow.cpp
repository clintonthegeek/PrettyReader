#include "mainwindow.h"

#include <KActionCollection>
#include <KActionMenu>
#include <KLocalizedString>
#include <KRecentFilesAction>
#include <KStandardAction>
#include <KToolBar>
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
#include "hyphenator.h"
#include "metadatastore.h"
#include "rtfexporter.h"
#include "shortwords.h"
#include "tocwidget.h"
#include "printcontroller.h"
#include "stylemanager.h"
#include "styledockwidget.h"
#include "themepickerdock.h"
#include "typographytheme.h"
#include "typographythememanager.h"
#include "colorpalette.h"
#include "palettemanager.h"
#include "themecomposer.h"
#include "thememanager.h"
#include "footnotestyle.h"
#include "preferencesdialog.h"
#include "prettyreadersettings.h"

// PDF rendering pipeline (Phase 4)
#include "contentbuilder.h"
#include "fontmanager.h"
#include "textshaper.h"
#include "layoutengine.h"
#include "pdfgenerator.h"
#include "pdfexportdialog.h"
#include "pdfexportoptions.h"
#include "contentfilter.h"
#include "pagerangeparser.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCloseEvent>
#include <QFile>
#include <QFileDialog>
#include <QJsonObject>
#include <QJsonArray>
#include <QLabel>
#include <QMenuBar>
#include <QAbstractTextDocumentLayout>
#include <QSlider>
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
        if (view) {
            int zoom = view->zoomPercent();
            if (m_zoomSpinBox) {
                const QSignalBlocker blocker(m_zoomSpinBox);
                m_zoomSpinBox->setValue(zoom);
            }
            if (m_zoomSlider) {
                const QSignalBlocker blocker(m_zoomSlider);
                m_zoomSlider->setValue(zoom);
            }
        }

        // A2: Update file browser to show current file's directory
        // A6: Update status bar file path
        auto *tab = currentDocumentTab();
        if (tab && !tab->filePath().isEmpty()) {
            QFileInfo fi(tab->filePath());
            m_fileBrowserWidget->setRootPath(fi.absolutePath());
            if (m_filePathLabel)
                m_filePathLabel->setText(tab->filePath());
        } else {
            if (m_filePathLabel)
                m_filePathLabel->clear();
        }
    });

    m_themeManager = new ThemeManager(this);
    m_paletteManager = new PaletteManager(this);
    m_typographyThemeManager = new TypographyThemeManager(this);
    m_themeComposer = new ThemeComposer(m_themeManager, this);
    m_metadataStore = new MetadataStore(this);

    // Typography engines
    m_hyphenator = new Hyphenator();
    m_shortWords = new ShortWords();

    // PDF rendering pipeline (created once, reused across rebuilds)
    m_fontManager = new FontManager();
    m_textShaper = new TextShaper(m_fontManager);

    // Load bundled symbol fallback font for glyphs missing in body fonts
    FontFace *fallback = m_fontManager->loadFontFromPath(
        QStringLiteral(":/fonts/PrettySymbolsFallback.ttf"));
    m_textShaper->setFallbackFont(fallback);

    // Apply settings
    auto *settings = PrettyReaderSettings::self();
    if (settings->hyphenationEnabled() || settings->hyphenateJustifiedText()) {
        m_hyphenator->loadDictionary(settings->hyphenationLanguage());
        m_hyphenator->setMinWordLength(settings->hyphenationMinWordLength());
    }
    if (settings->shortWordsEnabled()) {
        m_shortWords->setLanguage(settings->hyphenationLanguage());
    }

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

    // A6: File path label (left-justified, auto-hides for temporary messages)
    m_filePathLabel = new QLabel;
    m_filePathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    statusBar()->addWidget(m_filePathLabel, 1);

    // A5: Zoom slider + spinbox on status bar (permanent, right side)
    m_zoomSlider = new QSlider(Qt::Horizontal);
    m_zoomSlider->setRange(25, 400);
    m_zoomSlider->setValue(100);
    m_zoomSlider->setFixedWidth(120);
    m_zoomSlider->setToolTip(i18n("Zoom level"));
    statusBar()->addPermanentWidget(m_zoomSlider);

    m_zoomSpinBox = new QSpinBox;
    m_zoomSpinBox->setRange(25, 400);
    m_zoomSpinBox->setSuffix(QStringLiteral("%"));
    m_zoomSpinBox->setValue(100);
    m_zoomSpinBox->setFixedWidth(80);
    m_zoomSpinBox->setToolTip(i18n("Zoom level"));
    statusBar()->addPermanentWidget(m_zoomSpinBox);

    // Bidirectional sync between slider and spinbox (with signal blockers)
    connect(m_zoomSlider, &QSlider::valueChanged, this, [this](int value) {
        const QSignalBlocker b(m_zoomSpinBox);
        m_zoomSpinBox->setValue(value);
        auto *view = currentDocumentView();
        if (view) view->setZoomPercent(value);
    });
    connect(m_zoomSpinBox, qOverload<int>(&QSpinBox::valueChanged), this, [this](int value) {
        const QSignalBlocker b(m_zoomSlider);
        m_zoomSlider->setValue(value);
        auto *view = currentDocumentView();
        if (view) view->setZoomPercent(value);
    });

    statusBar()->showMessage(i18n("Ready"));

    setMinimumSize(800, 600);
    resize(1200, 800);

    // Load default typography theme + color palette so the style tree is populated
    {
        QStringList typoThemes = m_typographyThemeManager->availableThemes();
        if (!typoThemes.isEmpty()) {
            TypographyTheme theme = m_typographyThemeManager->theme(
                typoThemes.contains(QStringLiteral("default")) ? QStringLiteral("default") : typoThemes.first());
            m_themeComposer->setTypographyTheme(theme);
        }
        QStringList palettes = m_paletteManager->availablePalettes();
        if (!palettes.isEmpty()) {
            ColorPalette palette = m_paletteManager->palette(
                palettes.contains(QStringLiteral("default-light")) ? QStringLiteral("default-light") : palettes.first());
            m_themeComposer->setColorPalette(palette);
        }
        onCompositionApplied();
    }

    restoreSession();

    // A1: Fresh launch = TOC open by default (saved session state takes priority)
    if (m_leftSidebar->isCollapsed())
        m_leftSidebar->showPanel(m_tocTabId);
}

MainWindow::~MainWindow()
{
    delete m_hyphenator;
    delete m_shortWords;
    delete m_textShaper;
    delete m_fontManager;
}

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

    connect(m_tocWidget, &TocWidget::headingNavigate,
            this, [this](int page, qreal yOffset) {
        auto *view = currentDocumentView();
        if (view)
            view->scrollToPosition(page, yOffset);
    });

    // Right sidebar: Theme + Style + Page Layout
    m_rightSidebar = new Sidebar(Sidebar::Right, this);

    // 1. Theme Picker (first panel)
    m_themePickerDock = new ThemePickerDock(
        m_themeManager, m_paletteManager, m_typographyThemeManager, m_themeComposer, this);
    auto *themeView = new ToolView(i18n("Theme"), m_themePickerDock);
    m_themePickerTabId = m_rightSidebar->addPanel(
        themeView, QIcon::fromTheme(QStringLiteral("color-management")), i18n("Theme"));

    // 2. Style (simplified -- tree + property editors only)
    m_styleDockWidget = new StyleDockWidget(this);
    auto *styleView = new ToolView(i18n("Style"), m_styleDockWidget);
    m_styleTabId = m_rightSidebar->addPanel(
        styleView, QIcon::fromTheme(QStringLiteral("preferences-desktop-font")), i18n("Style"));

    // 3. Page Layout
    m_pageLayoutWidget = new PageLayoutWidget(this);
    auto *pageView = new ToolView(i18n("Page"), m_pageLayoutWidget);
    m_pageLayoutTabId = m_rightSidebar->addPanel(
        pageView, QIcon::fromTheme(QStringLiteral("document-properties")), i18n("Page"));

    // Wire signals
    connect(m_themePickerDock, &ThemePickerDock::compositionApplied,
            this, &MainWindow::onCompositionApplied);
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
    KStandardAction::preferences(this, &MainWindow::showPreferences, ac);

    // File > Open
    auto *openAction = KStandardAction::open(this, &MainWindow::onFileOpen, ac);
    openAction->setPriority(QAction::LowPriority);

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

    // File > Export RTF
    auto *exportRtf = ac->addAction(QStringLiteral("file_export_rtf"));
    exportRtf->setText(i18n("Export as &RTF..."));
    exportRtf->setIcon(QIcon::fromTheme(QStringLiteral("document-export")));
    connect(exportRtf, &QAction::triggered, this, &MainWindow::onFileExportRtf);

    // File > Print
    auto *printAction = KStandardAction::print(this, &MainWindow::onFilePrint, ac);
    printAction->setPriority(QAction::LowPriority);

    // File > Close
    auto *closeAction = KStandardAction::close(this, &MainWindow::onFileClose, ac);
    Q_UNUSED(closeAction);

    // View > Zoom
    auto *zoomInAction = KStandardAction::zoomIn(this, &MainWindow::onZoomIn, ac);
    zoomInAction->setPriority(QAction::LowPriority);
    auto *zoomOutAction = KStandardAction::zoomOut(this, &MainWindow::onZoomOut, ac);
    zoomOutAction->setPriority(QAction::LowPriority);

    auto *fitWidth = ac->addAction(QStringLiteral("view_zoom_fit_width"));
    fitWidth->setText(i18n("Fit &Width"));
    fitWidth->setIcon(QIcon::fromTheme(QStringLiteral("zoom-fit-width")));
    connect(fitWidth, &QAction::triggered, this, &MainWindow::onFitWidth);

    auto *fitPage = ac->addAction(QStringLiteral("view_zoom_fit_page"));
    fitPage->setText(i18n("Fit &Page"));
    fitPage->setIcon(QIcon::fromTheme(QStringLiteral("zoom-fit-page")));
    connect(fitPage, &QAction::triggered, this, &MainWindow::onFitPage);

    // View > Render Mode (Print vs Web)
    auto *renderModeGroup = new QActionGroup(this);
    renderModeGroup->setExclusive(true);

    m_printViewAction = ac->addAction(QStringLiteral("view_print_mode"));
    m_printViewAction->setText(i18n("&Print View"));
    m_printViewAction->setIcon(QIcon::fromTheme(QStringLiteral("document-print-preview")));
    m_printViewAction->setCheckable(true);
    m_printViewAction->setChecked(!PrettyReaderSettings::self()->useWebView());
    m_printViewAction->setActionGroup(renderModeGroup);
    connect(m_printViewAction, &QAction::triggered, this, [this]() {
        PrettyReaderSettings::self()->setUseWebView(false);
        PrettyReaderSettings::self()->save();
        onRenderModeChanged();
    });

    m_webViewAction = ac->addAction(QStringLiteral("view_web_mode"));
    m_webViewAction->setText(i18n("&Web View"));
    m_webViewAction->setIcon(QIcon::fromTheme(QStringLiteral("text-html")));
    m_webViewAction->setCheckable(true);
    m_webViewAction->setChecked(PrettyReaderSettings::self()->useWebView());
    m_webViewAction->setActionGroup(renderModeGroup);
    connect(m_webViewAction, &QAction::triggered, this, [this]() {
        PrettyReaderSettings::self()->setUseWebView(true);
        PrettyReaderSettings::self()->save();
        onRenderModeChanged();
    });

    // View > Mode (exclusive action group)
    auto *viewModeGroup = new QActionGroup(this);
    viewModeGroup->setExclusive(true);

    auto *continuous = ac->addAction(QStringLiteral("view_continuous"));
    continuous->setText(i18n("&Continuous Scroll"));
    continuous->setIcon(QIcon::fromTheme(QStringLiteral("view-pages-continuous")));
    continuous->setCheckable(true);
    continuous->setChecked(true);
    continuous->setActionGroup(viewModeGroup);
    connect(continuous, &QAction::triggered, this, [this]() {
        auto *view = currentDocumentView();
        if (view)
            view->setViewMode(DocumentView::Continuous);
    });

    auto *singlePage = ac->addAction(QStringLiteral("view_single_page"));
    singlePage->setText(i18n("&Single Page"));
    singlePage->setIcon(QIcon::fromTheme(QStringLiteral("view-paged-symbolic")));
    singlePage->setCheckable(true);
    singlePage->setActionGroup(viewModeGroup);
    connect(singlePage, &QAction::triggered, this, [this]() {
        auto *view = currentDocumentView();
        if (view)
            view->setViewMode(DocumentView::SinglePage);
    });

    auto *facingPages = ac->addAction(QStringLiteral("view_facing_pages"));
    facingPages->setText(i18n("&Facing Pages"));
    facingPages->setIcon(QIcon::fromTheme(QStringLiteral("view-pages-facing")));
    facingPages->setCheckable(true);
    facingPages->setActionGroup(viewModeGroup);
    connect(facingPages, &QAction::triggered, this, [this]() {
        auto *view = currentDocumentView();
        if (view)
            view->setViewMode(DocumentView::FacingPages);
    });

    auto *facingFirstAlone = ac->addAction(QStringLiteral("view_facing_first_alone"));
    facingFirstAlone->setText(i18n("Facing Pages (First &Alone)"));
    facingFirstAlone->setIcon(QIcon::fromTheme(QStringLiteral("view-pages-facing-first-centered")));
    facingFirstAlone->setCheckable(true);
    facingFirstAlone->setActionGroup(viewModeGroup);
    connect(facingFirstAlone, &QAction::triggered, this, [this]() {
        auto *view = currentDocumentView();
        if (view)
            view->setViewMode(DocumentView::FacingPagesFirstAlone);
    });

    auto *continuousFacing = ac->addAction(QStringLiteral("view_continuous_facing"));
    continuousFacing->setText(i18n("Continuous F&acing"));
    continuousFacing->setIcon(QIcon::fromTheme(QStringLiteral("view-pages-facing-symbolic")));
    continuousFacing->setCheckable(true);
    continuousFacing->setActionGroup(viewModeGroup);
    connect(continuousFacing, &QAction::triggered, this, [this]() {
        auto *view = currentDocumentView();
        if (view)
            view->setViewMode(DocumentView::ContinuousFacing);
    });

    auto *continuousFacingFirstAlone = ac->addAction(QStringLiteral("view_continuous_facing_first_alone"));
    continuousFacingFirstAlone->setText(i18n("Continuous Facing (First A&lone)"));
    continuousFacingFirstAlone->setIcon(QIcon::fromTheme(QStringLiteral("view-pages-facing-first-centered")));
    continuousFacingFirstAlone->setCheckable(true);
    continuousFacingFirstAlone->setActionGroup(viewModeGroup);
    connect(continuousFacingFirstAlone, &QAction::triggered, this, [this]() {
        auto *view = currentDocumentView();
        if (view)
            view->setViewMode(DocumentView::ContinuousFacingFirstAlone);
    });

    // A4: Page Arrangement submenu (collects the 6 view mode actions)
    auto *arrangementMenu = new KActionMenu(
        QIcon::fromTheme(QStringLiteral("view-list-details")),
        i18n("Page &Arrangement"), this);
    ac->addAction(QStringLiteral("view_page_arrangement"), arrangementMenu);
    m_pageArrangementMenu = arrangementMenu;
    arrangementMenu->addAction(continuous);
    arrangementMenu->addAction(singlePage);
    arrangementMenu->addAction(facingPages);
    arrangementMenu->addAction(facingFirstAlone);
    arrangementMenu->addAction(continuousFacing);
    arrangementMenu->addAction(continuousFacingFirstAlone);

    // Go > Navigation
    auto *prevPage = ac->addAction(QStringLiteral("go_previous_page"));
    prevPage->setText(i18n("&Previous Page"));
    prevPage->setIcon(QIcon::fromTheme(QStringLiteral("go-previous")));
    prevPage->setPriority(QAction::LowPriority);
    ac->setDefaultShortcut(prevPage, QKeySequence(Qt::Key_PageUp));
    connect(prevPage, &QAction::triggered, this, [this]() {
        auto *view = currentDocumentView();
        if (view)
            view->previousPage();
    });

    auto *nextPage = ac->addAction(QStringLiteral("go_next_page"));
    nextPage->setText(i18n("&Next Page"));
    nextPage->setIcon(QIcon::fromTheme(QStringLiteral("go-next")));
    nextPage->setPriority(QAction::LowPriority);
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

    auto *toggleTheme = ac->addAction(QStringLiteral("view_toggle_theme"));
    toggleTheme->setText(i18n("&Theme Panel"));
    toggleTheme->setIcon(QIcon::fromTheme(QStringLiteral("color-management")));
    toggleTheme->setCheckable(true);
    connect(toggleTheme, &QAction::triggered, this, [this](bool checked) {
        if (checked)
            m_rightSidebar->showPanel(m_themePickerTabId);
        else
            m_rightSidebar->hidePanel(m_themePickerTabId);
    });
    connect(m_rightSidebar, &Sidebar::panelVisibilityChanged,
            this, [this, toggleTheme](int tabId, bool visible) {
        if (tabId == m_themePickerTabId)
            toggleTheme->setChecked(visible);
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

    // B1: Cursor mode toggle actions
    auto *handTool = ac->addAction(QStringLiteral("tool_hand"));
    handTool->setText(i18n("&Hand Tool"));
    handTool->setIcon(QIcon::fromTheme(QStringLiteral("transform-browse")));
    handTool->setCheckable(true);
    handTool->setChecked(true);
    ac->setDefaultShortcut(handTool, QKeySequence(Qt::CTRL | Qt::Key_1));

    auto *selectTool = ac->addAction(QStringLiteral("tool_selection"));
    selectTool->setText(i18n("&Text Selection"));
    selectTool->setIcon(QIcon::fromTheme(QStringLiteral("edit-select-text")));
    selectTool->setCheckable(true);
    ac->setDefaultShortcut(selectTool, QKeySequence(Qt::CTRL | Qt::Key_2));

    auto *toolGroup = new QActionGroup(this);
    toolGroup->setExclusive(true);
    toolGroup->addAction(handTool);
    toolGroup->addAction(selectTool);

    connect(handTool, &QAction::triggered, this, [this]() {
        auto *view = currentDocumentView();
        if (view) view->setCursorMode(DocumentView::HandTool);
    });
    connect(selectTool, &QAction::triggered, this, [this]() {
        auto *view = currentDocumentView();
        if (view) view->setCursorMode(DocumentView::SelectionTool);
    });

    // B2: Copy action (Ctrl+C)
    KStandardAction::copy(this, [this]() {
        auto *view = currentDocumentView();
        if (view) view->copySelection();
    }, ac);

    // Copy as Styled Text (RTF)
    auto *copyRtf = ac->addAction(QStringLiteral("edit_copy_rtf"));
    copyRtf->setText(i18n("Copy as &Styled Text"));
    copyRtf->setIcon(QIcon::fromTheme(QStringLiteral("edit-copy")));
    ac->setDefaultShortcut(copyRtf, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_C));
    connect(copyRtf, &QAction::triggered, this, [this]() {
        auto *view = currentDocumentView();
        if (view) view->copySelectionAsRtf();
    });

    // Copy with Style Options (filtered RTF)
    auto *copyComplex = ac->addAction(QStringLiteral("edit_copy_complex"));
    copyComplex->setText(i18n("Copy with Style &Options..."));
    copyComplex->setIcon(QIcon::fromTheme(QStringLiteral("edit-copy")));
    ac->setDefaultShortcut(copyComplex, QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_C));
    connect(copyComplex, &QAction::triggered, this, [this]() {
        auto *view = currentDocumentView();
        if (view) view->copySelectionAsComplexRtf();
    });

    // Copy as Markdown
    auto *copyMd = ac->addAction(QStringLiteral("edit_copy_markdown"));
    copyMd->setText(i18n("Copy as &Markdown"));
    copyMd->setIcon(QIcon::fromTheme(QStringLiteral("text-x-script")));
    connect(copyMd, &QAction::triggered, this, [this]() {
        auto *view = currentDocumentView();
        if (view) view->copySelectionAsMarkdown();
    });

    setupGUI(Default, QStringLiteral("prettyreaderui.rc"));

    // Show text labels by default; LowPriority actions get icon-only
    toolBar()->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
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
    if (!view) {
        statusBar()->showMessage(i18n("No document to export."), 3000);
        return;
    }

    if (view->isPdfMode()) {
        auto *tab = currentDocumentTab();
        if (!tab)
            return;

        QString filePath = tab->filePath();
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

        StyleManager *editingSm = m_styleDockWidget->currentStyleManager();
        StyleManager *styleManager;
        if (editingSm) {
            styleManager = editingSm->clone(this);
        } else {
            styleManager = new StyleManager(this);
            m_themeComposer->compose(styleManager);
        }

        QFileInfo fi(filePath);
        PageLayout pl = m_pageLayoutWidget->currentPageLayout();

        // Build content (needed for heading tree + page count)
        ContentBuilder contentBuilder;
        contentBuilder.setBasePath(fi.absolutePath());
        contentBuilder.setStyleManager(styleManager);
        if (PrettyReaderSettings::self()->hyphenationEnabled()
            || PrettyReaderSettings::self()->hyphenateJustifiedText())
            contentBuilder.setHyphenator(m_hyphenator);
        if (PrettyReaderSettings::self()->shortWordsEnabled())
            contentBuilder.setShortWords(m_shortWords);
        contentBuilder.setFootnoteStyle(styleManager->footnoteStyle());
        Content::Document contentDoc = contentBuilder.build(markdown);
        view->applyLanguageOverrides(contentDoc);

        // Pre-layout to get page count for dialog
        m_fontManager->resetUsage();
        Layout::Engine preLayoutEngine(m_fontManager, m_textShaper);
        preLayoutEngine.setHyphenateJustifiedText(
            PrettyReaderSettings::self()->hyphenateJustifiedText());
        Layout::LayoutResult preLayout = preLayoutEngine.layout(contentDoc, pl);
        int pageCount = preLayout.pages.size();

        // Load saved options from KConfig
        PdfExportOptions opts;
        auto *settings = PrettyReaderSettings::self();
        opts.author = settings->pdfAuthor();
        opts.markdownCopy = settings->pdfMarkdownCopy();
        opts.unwrapParagraphs = settings->pdfUnwrapParagraphs();
        opts.includeBookmarks = settings->pdfIncludeBookmarks();
        opts.bookmarkMaxDepth = settings->pdfBookmarkMaxDepth();
        opts.initialView = static_cast<PdfExportOptions::InitialView>(
            settings->pdfInitialView());
        opts.pageLayout = static_cast<PdfExportOptions::PageLayout>(
            settings->pdfPageLayout());

        // Overlay per-document options from MetadataStore
        QJsonObject perDoc = m_metadataStore->load(filePath);
        if (perDoc.contains(QStringLiteral("pdfExportOptions"))) {
            QJsonObject saved = perDoc[QStringLiteral("pdfExportOptions")].toObject();
            if (saved.contains(QStringLiteral("title")))
                opts.title = saved[QStringLiteral("title")].toString();
            if (saved.contains(QStringLiteral("author")))
                opts.author = saved[QStringLiteral("author")].toString();
            if (saved.contains(QStringLiteral("subject")))
                opts.subject = saved[QStringLiteral("subject")].toString();
            if (saved.contains(QStringLiteral("keywords")))
                opts.keywords = saved[QStringLiteral("keywords")].toString();
            if (saved.contains(QStringLiteral("pageRangeExpr")))
                opts.pageRangeExpr = saved[QStringLiteral("pageRangeExpr")].toString();
            if (saved.contains(QStringLiteral("excludedHeadingIndices"))) {
                QJsonArray arr = saved[QStringLiteral("excludedHeadingIndices")].toArray();
                for (const auto &v : arr)
                    opts.excludedHeadingIndices.insert(v.toInt());
            }
        }

        // Show export dialog
        PdfExportDialog dlg(contentDoc, pageCount, fi.baseName(), this);
        dlg.setOptions(opts);
        dlg.setHasNonWhiteBackgrounds(m_themeComposer->currentPalette().hasNonWhiteBackgrounds());
        if (dlg.exec() != QDialog::Accepted)
            return;

        opts = dlg.options();

        // Save global defaults to KConfig
        settings->setPdfAuthor(opts.author);
        settings->setPdfMarkdownCopy(opts.markdownCopy);
        settings->setPdfUnwrapParagraphs(opts.unwrapParagraphs);
        settings->setPdfIncludeBookmarks(opts.includeBookmarks);
        settings->setPdfBookmarkMaxDepth(opts.bookmarkMaxDepth);
        settings->setPdfInitialView(static_cast<int>(opts.initialView));
        settings->setPdfPageLayout(static_cast<int>(opts.pageLayout));
        settings->save();

        // Save per-document options to MetadataStore
        QJsonObject docOpts;
        docOpts[QStringLiteral("title")] = opts.title;
        docOpts[QStringLiteral("author")] = opts.author;
        docOpts[QStringLiteral("subject")] = opts.subject;
        docOpts[QStringLiteral("keywords")] = opts.keywords;
        docOpts[QStringLiteral("pageRangeExpr")] = opts.pageRangeExpr;
        QJsonArray excludedArr;
        for (int idx : opts.excludedHeadingIndices)
            excludedArr.append(idx);
        docOpts[QStringLiteral("excludedHeadingIndices")] = excludedArr;
        m_metadataStore->setValue(filePath, QStringLiteral("pdfExportOptions"), docOpts);

        // File save dialog
        QString path = QFileDialog::getSaveFileName(
            this, i18n("Export as PDF"),
            QString(), i18n("PDF Files (*.pdf)"));
        if (path.isEmpty())
            return;

        // Filter content by excluded sections
        Content::Document filteredDoc = contentDoc;
        if (opts.sectionsModified && !opts.excludedHeadingIndices.isEmpty())
            filteredDoc = ContentFilter::filterSections(contentDoc, opts.excludedHeadingIndices);

        // Layout with filtered content
        m_fontManager->resetUsage();
        Layout::Engine layoutEngine(m_fontManager, m_textShaper);
        layoutEngine.setHyphenateJustifiedText(
            PrettyReaderSettings::self()->hyphenateJustifiedText());
        if (opts.markdownCopy)
            layoutEngine.setMarkdownDecorations(true);
        Layout::LayoutResult layoutResult = layoutEngine.layout(filteredDoc, pl);

        // Filter pages by range
        if (opts.pageRangeModified && !opts.pageRangeExpr.isEmpty()) {
            auto rangeResult = PageRangeParser::parse(opts.pageRangeExpr, layoutResult.pages.size());
            if (rangeResult.valid && rangeResult.pages.size() < layoutResult.pages.size()) {
                QList<Layout::Page> filteredPages;
                for (int i = 0; i < layoutResult.pages.size(); ++i) {
                    if (rangeResult.pages.contains(i + 1))  // 1-based
                        filteredPages.append(layoutResult.pages[i]);
                }
                layoutResult.pages = filteredPages;
                // Renumber pages
                for (int i = 0; i < layoutResult.pages.size(); ++i)
                    layoutResult.pages[i].pageNumber = i;
            }
        }

        // Generate PDF with options
        PdfGenerator pdfGen(m_fontManager);
        pdfGen.setMaxJustifyGap(settings->maxJustifyGap());
        pdfGen.setExportOptions(opts);
        if (pdfGen.generateToFile(layoutResult, pl, fi.baseName(), path)) {
            statusBar()->showMessage(i18n("Exported to %1", path), 3000);
        } else {
            statusBar()->showMessage(i18n("Failed to export PDF."), 3000);
        }
    } else {
        // Legacy pipeline
        if (!view->document()) {
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
}

void MainWindow::onFileExportRtf()
{
    auto *view = currentDocumentView();
    if (!view || !view->document()) {
        statusBar()->showMessage(i18n("No document to export."), 3000);
        return;
    }

    QString path = QFileDialog::getSaveFileName(
        this, i18n("Export as RTF"),
        QString(), i18n("RTF Files (*.rtf)"));
    if (path.isEmpty())
        return;

    RtfExporter exporter;
    if (exporter.exportToFile(view->document(), path)) {
        statusBar()->showMessage(i18n("Exported to %1", path), 3000);
    } else {
        statusBar()->showMessage(i18n("Failed to export RTF."), 3000);
    }
}

void MainWindow::onFilePrint()
{
    auto *view = currentDocumentView();
    if (!view) {
        statusBar()->showMessage(i18n("No document to print."), 3000);
        return;
    }

    if (view->isPdfMode()) {
        // New pipeline: use PrintController with Poppler-based printing
        auto *controller = new PrintController(nullptr, this);
        controller->setPdfData(view->pdfData());
        PageLayout pl = m_pageLayoutWidget->currentPageLayout();
        controller->setPageLayout(pl);
        QString title = m_tabWidget->tabText(m_tabWidget->currentIndex());
        controller->setFileName(title);
        controller->print(this);
        delete controller;
    } else {
        // Legacy pipeline
        if (!view->document()) {
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
}

void MainWindow::onFileClose()
{
    int index = m_tabWidget->currentIndex();
    if (index >= 0) {
        m_tabWidget->removeTab(index);
    }
}

void MainWindow::onCompositionApplied()
{
    // ThemePickerDock already called compose() on the editing styles
    m_styleDockWidget->refreshTreeModel();

    // Pick up page layout from theme manager (applyStyleOverrides may have set it)
    PageLayout pl = m_themeManager->themePageLayout();
    // If the theme didn't specify a page layout, start from the current one
    if (pl.pageSizeId == QPageSize::A4 && pl.margins.isNull())
        pl = m_pageLayoutWidget->currentPageLayout();

    // Update page background from palette
    QColor pageBg = m_themeComposer->currentPalette().pageBackground();
    if (pageBg.isValid())
        pl.pageBackground = pageBg;

    m_pageLayoutWidget->setPageLayout(pl);
    auto *view = currentDocumentView();
    if (view)
        view->setPageLayout(pl);

    rebuildCurrentDocument();
}

void MainWindow::onStyleOverrideChanged()
{
    // Update page background from the current palette
    QColor pageBg = m_themeComposer->currentPalette().pageBackground();
    if (pageBg.isValid()) {
        PageLayout pl = m_pageLayoutWidget->currentPageLayout();
        pl.pageBackground = pageBg;
        m_pageLayoutWidget->setPageLayout(pl);
        auto *view = currentDocumentView();
        if (view)
            view->setPageLayout(pl);
    }

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

void MainWindow::showPreferences()
{
    if (KConfigDialog::showDialog(QStringLiteral("settings")))
        return;

    auto *dialog = new PrettyReaderConfigDialog(this);
    connect(dialog, &KConfigDialog::settingsChanged,
            this, &MainWindow::onSettingsChanged);
    dialog->show();
}

void MainWindow::onSettingsChanged()
{
    auto *settings = PrettyReaderSettings::self();

    // Reconfigure hyphenator
    if (settings->hyphenationEnabled() || settings->hyphenateJustifiedText()) {
        m_hyphenator->loadDictionary(settings->hyphenationLanguage());
        m_hyphenator->setMinWordLength(settings->hyphenationMinWordLength());
    }

    // Reconfigure short words
    if (settings->shortWordsEnabled())
        m_shortWords->setLanguage(settings->hyphenationLanguage());

    rebuildCurrentDocument();
}

void MainWindow::onRenderModeChanged()
{
    bool webMode = PrettyReaderSettings::self()->useWebView();

    // Enable/disable page arrangement (not meaningful in web mode)
    if (m_pageArrangementMenu)
        m_pageArrangementMenu->setEnabled(!webMode);

    auto *view = currentDocumentView();
    if (view)
        view->setRenderMode(webMode ? DocumentView::WebMode : DocumentView::PrintMode);

    rebuildCurrentDocument();
}

void MainWindow::saveSession()
{
    KConfigGroup group(KSharedConfig::openConfig(),
                       QStringLiteral("Session"));

    // Save sidebar state (A3: no longer saving open files or active tab)
    group.writeEntry("LeftSidebarCollapsed", m_leftSidebar->isCollapsed());
    group.writeEntry("RightSidebarCollapsed", m_rightSidebar->isCollapsed());
    // Save which left panel was active (Files=0, TOC=1) so we restore the right one
    if (!m_leftSidebar->isCollapsed()) {
        if (m_leftSidebar->isPanelVisible(m_tocTabId))
            group.writeEntry("LeftActivePanel", QStringLiteral("toc"));
        else
            group.writeEntry("LeftActivePanel", QStringLiteral("files"));
    }
    if (!m_rightSidebar->isCollapsed()) {
        if (m_rightSidebar->isPanelVisible(m_themePickerTabId))
            group.writeEntry("RightActivePanel", QStringLiteral("theme"));
        else if (m_rightSidebar->isPanelVisible(m_styleTabId))
            group.writeEntry("RightActivePanel", QStringLiteral("style"));
        else
            group.writeEntry("RightActivePanel", QStringLiteral("page"));
    }
    if (m_splitter)
        group.writeEntry("SplitterSizes", m_splitter->sizes());

    // Save current typography theme + color scheme
    group.writeEntry("TypographyTheme", m_themePickerDock->currentTypographyThemeId());
    group.writeEntry("ColorScheme", m_themePickerDock->currentColorSchemeId());

    group.sync();
}

void MainWindow::restoreSession()
{
    KConfigGroup group(KSharedConfig::openConfig(),
                       QStringLiteral("Session"));

    // A3: No longer restoring open files or active tab

    // Restore sidebar state
    bool leftCollapsed = group.readEntry("LeftSidebarCollapsed", true);
    bool rightCollapsed = group.readEntry("RightSidebarCollapsed", true);

    // Expand sidebars that were open last session (unlocks width constraints)
    if (!leftCollapsed) {
        QString leftPanel = group.readEntry("LeftActivePanel", QStringLiteral("toc"));
        if (leftPanel == QLatin1String("files"))
            m_leftSidebar->showPanel(m_filesBrowserTabId);
        else
            m_leftSidebar->showPanel(m_tocTabId);
    }
    if (!rightCollapsed) {
        QString rightPanel = group.readEntry("RightActivePanel", QStringLiteral("style"));
        if (rightPanel == QLatin1String("theme"))
            m_rightSidebar->showPanel(m_themePickerTabId);
        else if (rightPanel == QLatin1String("page"))
            m_rightSidebar->showPanel(m_pageLayoutTabId);
        else
            m_rightSidebar->showPanel(m_styleTabId);
    }

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

    // Restore typography theme + color scheme
    QString typoId = group.readEntry("TypographyTheme", QStringLiteral("default"));
    QString colorId = group.readEntry("ColorScheme", QStringLiteral("default-light"));

    bool changed = false;
    TypographyTheme theme = m_typographyThemeManager->theme(typoId);
    if (!theme.id.isEmpty()) {
        m_themeComposer->setTypographyTheme(theme);
        changed = true;
    }

    ColorPalette palette = m_paletteManager->palette(colorId);
    if (!palette.id.isEmpty()) {
        m_themeComposer->setColorPalette(palette);
        changed = true;
    }

    m_themePickerDock->syncPickersFromComposer();

    if (changed)
        onCompositionApplied();
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
        m_themeComposer->compose(styleManager);
    }

    auto *view = tab->documentView();
    if (!view)
        return;

    ViewState state = view->saveViewState();
    PageLayout pl = m_pageLayoutWidget->currentPageLayout();
    QFileInfo fi(filePath);

    if (PrettyReaderSettings::self()->usePdfRenderer()) {
        // --- New rendering pipeline (shared content building) ---
        ContentBuilder contentBuilder;
        contentBuilder.setBasePath(fi.absolutePath());
        contentBuilder.setStyleManager(styleManager);
        if (PrettyReaderSettings::self()->hyphenationEnabled()
            || PrettyReaderSettings::self()->hyphenateJustifiedText())
            contentBuilder.setHyphenator(m_hyphenator);
        if (PrettyReaderSettings::self()->shortWordsEnabled())
            contentBuilder.setShortWords(m_shortWords);
        contentBuilder.setFootnoteStyle(styleManager->footnoteStyle());
        Content::Document contentDoc = contentBuilder.build(markdown);

        view->applyLanguageOverrides(contentDoc);

        m_fontManager->resetUsage();

        Layout::Engine layoutEngine(m_fontManager, m_textShaper);
        layoutEngine.setHyphenateJustifiedText(
            PrettyReaderSettings::self()->hyphenateJustifiedText());

        if (PrettyReaderSettings::self()->useWebView()) {
            // --- Web view pipeline ---
            qreal availWidth = view->viewport()->width() - 2 * DocumentView::kSceneMargin;
            qreal zoomFactor = view->zoomPercent() / 100.0;
            if (zoomFactor > 0)
                availWidth /= zoomFactor;
            if (availWidth < 200)
                availWidth = 200;

            Layout::ContinuousLayoutResult webResult =
                layoutEngine.layoutContinuous(contentDoc, availWidth);

            // Build heading positions (absolute y from source map)
            QList<HeadingPosition> headingPositions;
            for (const auto &block : contentDoc.blocks) {
                const auto *heading = std::get_if<Content::Heading>(&block);
                if (!heading || heading->level < 1 || heading->level > 6)
                    continue;
                HeadingPosition hp;
                hp.page = 0;
                hp.sourceLine = heading->source.startLine;
                if (heading->source.startLine > 0) {
                    for (const auto &entry : webResult.sourceMap) {
                        if (entry.startLine == heading->source.startLine
                            && entry.endLine == heading->source.endLine) {
                            hp.yOffset = entry.rect.top();
                            break;
                        }
                    }
                }
                headingPositions.append(hp);
            }

            // TOC from content model
            m_tocWidget->buildFromContentModel(contentDoc, webResult.sourceMap);

            view->setWebFontManager(m_fontManager);
            view->setHeadingPositions(headingPositions);
            view->setSourceData(contentBuilder.processedMarkdown(), webResult.sourceMap,
                                contentDoc, webResult.codeBlockRegions);
            view->setWebContent(std::move(webResult));
            view->setRenderMode(DocumentView::WebMode);
            view->restoreViewState(state);
            view->setDocumentInfo(fi.fileName(), fi.baseName());
            connect(view, &DocumentView::currentHeadingChanged,
                    m_tocWidget, &TocWidget::highlightHeading,
                    Qt::UniqueConnection);

            // Wire debounced relayout
            connect(view, &DocumentView::webRelayoutRequested,
                    this, &MainWindow::rebuildCurrentDocument,
                    Qt::UniqueConnection);
        } else {
            // --- PDF rendering pipeline ---
            Layout::LayoutResult layoutResult = layoutEngine.layout(contentDoc, pl);

            PdfGenerator pdfGen(m_fontManager);
            pdfGen.setMaxJustifyGap(PrettyReaderSettings::self()->maxJustifyGap());
            QByteArray pdf = pdfGen.generate(layoutResult, pl, fi.baseName());

            // Clear legacy document if switching pipelines
            QTextDocument *oldDoc = view->document();
            if (oldDoc) {
                view->setDocument(nullptr);
                delete oldDoc;
            }

            view->setPdfData(pdf);
            view->setSourceData(contentBuilder.processedMarkdown(), layoutResult.sourceMap, contentDoc,
                                layoutResult.codeBlockRegions);
            view->setRenderMode(DocumentView::PrintMode);
            view->restoreViewState(state);
            view->setDocumentInfo(fi.fileName(), fi.baseName());

            // Build TOC directly from content model + source map
            m_tocWidget->buildFromContentModel(contentDoc, layoutResult.sourceMap);

            // Pass heading positions to view for scroll-sync
            {
                QList<HeadingPosition> headingPositions;
                for (const auto &block : contentDoc.blocks) {
                    const auto *heading = std::get_if<Content::Heading>(&block);
                    if (!heading || heading->level < 1 || heading->level > 6)
                        continue;
                    HeadingPosition hp;
                    hp.sourceLine = heading->source.startLine;
                    if (heading->source.startLine > 0) {
                        for (const auto &entry : layoutResult.sourceMap) {
                            if (entry.startLine == heading->source.startLine
                                && entry.endLine == heading->source.endLine) {
                                hp.page = entry.pageNumber;
                                hp.yOffset = entry.rect.top();
                                break;
                            }
                        }
                    }
                    headingPositions.append(hp);
                }
                view->setHeadingPositions(headingPositions);
                connect(view, &DocumentView::currentHeadingChanged,
                        m_tocWidget, &TocWidget::highlightHeading,
                        Qt::UniqueConnection);
            }
        }
    } else {
        // --- Legacy QTextDocument pipeline ---
        auto *doc = new QTextDocument(this);
        auto *builder = new DocumentBuilder(doc, this);
        builder->setBasePath(fi.absolutePath());
        builder->setStyleManager(styleManager);
        if (PrettyReaderSettings::self()->hyphenationEnabled()
            || PrettyReaderSettings::self()->hyphenateJustifiedText())
            builder->setHyphenator(m_hyphenator);
        if (PrettyReaderSettings::self()->shortWordsEnabled())
            builder->setShortWords(m_shortWords);
        builder->setFootnoteStyle(styleManager->footnoteStyle());
        builder->build(markdown);

        CodeBlockHighlighter rebuildHighlighter;
        rebuildHighlighter.highlight(doc);

        QTextDocument *oldDoc = view->document();
        view->setDocument(doc);
        delete oldDoc;
        view->restoreViewState(state);
        view->setDocumentInfo(fi.fileName(), fi.baseName());

        m_tocWidget->buildFromDocument(doc);
    }

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
        m_themeComposer->compose(styleManager);
    }

    auto *tab = new DocumentTab(this);
    tab->setFilePath(filePath);
    tab->setSourceText(markdown);
    PageLayout openPl = m_pageLayoutWidget->currentPageLayout();
    tab->documentView()->setPageLayout(openPl);

    const bool webMode = PrettyReaderSettings::self()->useWebView();
    if (webMode)
        tab->documentView()->setRenderMode(DocumentView::WebMode);

    QFileInfo fi(filePath);

    // Load persisted code block language overrides from MetadataStore
    QJsonValue langVal = m_metadataStore->value(filePath, QStringLiteral("codeBlockLanguages"));
    if (langVal.isObject()) {
        QJsonObject langObj = langVal.toObject();
        QHash<QString, QString> overrides;
        for (auto it = langObj.begin(); it != langObj.end(); ++it)
            overrides.insert(it.key(), it.value().toString());
        tab->documentView()->setCodeBlockLanguageOverrides(overrides);
    }

    if (webMode) {
        // Web mode: defer to rebuildCurrentDocument() after tab is current
    } else if (PrettyReaderSettings::self()->usePdfRenderer()) {
        // --- New PDF rendering pipeline ---
        ContentBuilder contentBuilder;
        contentBuilder.setBasePath(fi.absolutePath());
        contentBuilder.setStyleManager(styleManager);
        if (PrettyReaderSettings::self()->hyphenationEnabled()
            || PrettyReaderSettings::self()->hyphenateJustifiedText())
            contentBuilder.setHyphenator(m_hyphenator);
        if (PrettyReaderSettings::self()->shortWordsEnabled())
            contentBuilder.setShortWords(m_shortWords);
        contentBuilder.setFootnoteStyle(styleManager->footnoteStyle());
        Content::Document contentDoc = contentBuilder.build(markdown);

        tab->documentView()->applyLanguageOverrides(contentDoc);

        m_fontManager->resetUsage();

        Layout::Engine layoutEngine(m_fontManager, m_textShaper);
        layoutEngine.setHyphenateJustifiedText(
            PrettyReaderSettings::self()->hyphenateJustifiedText());
        Layout::LayoutResult layoutResult = layoutEngine.layout(contentDoc, openPl);

        PdfGenerator pdfGen(m_fontManager);
        pdfGen.setMaxJustifyGap(PrettyReaderSettings::self()->maxJustifyGap());
        QByteArray pdf = pdfGen.generate(layoutResult, openPl, fi.baseName());

        tab->documentView()->setPdfData(pdf);
        tab->documentView()->setSourceData(
            contentBuilder.processedMarkdown(), layoutResult.sourceMap, contentDoc,
            layoutResult.codeBlockRegions);

        // Build TOC from content model (PDF mode)
        m_tocWidget->buildFromContentModel(contentDoc, layoutResult.sourceMap);

        // Pass heading positions to view for scroll-sync
        {
            QList<HeadingPosition> headingPositions;
            for (const auto &block : contentDoc.blocks) {
                const auto *heading = std::get_if<Content::Heading>(&block);
                if (!heading || heading->level < 1 || heading->level > 6)
                    continue;
                HeadingPosition hp;
                hp.sourceLine = heading->source.startLine;
                if (heading->source.startLine > 0) {
                    for (const auto &entry : layoutResult.sourceMap) {
                        if (entry.startLine == heading->source.startLine
                            && entry.endLine == heading->source.endLine) {
                            hp.page = entry.pageNumber;
                            hp.yOffset = entry.rect.top();
                            break;
                        }
                    }
                }
                headingPositions.append(hp);
            }
            tab->documentView()->setHeadingPositions(headingPositions);
            connect(tab->documentView(), &DocumentView::currentHeadingChanged,
                    m_tocWidget, &TocWidget::highlightHeading,
                    Qt::UniqueConnection);
        }
    } else {
        // --- Legacy QTextDocument pipeline ---
        auto *doc = new QTextDocument(this);
        auto *builder = new DocumentBuilder(doc, this);
        builder->setBasePath(fi.absolutePath());
        builder->setStyleManager(styleManager);
        if (PrettyReaderSettings::self()->hyphenationEnabled()
            || PrettyReaderSettings::self()->hyphenateJustifiedText())
            builder->setHyphenator(m_hyphenator);
        if (PrettyReaderSettings::self()->shortWordsEnabled())
            builder->setShortWords(m_shortWords);
        builder->setFootnoteStyle(styleManager->footnoteStyle());
        builder->build(markdown);

        CodeBlockHighlighter highlighter;
        highlighter.highlight(doc);

        tab->documentView()->setDocument(doc);
    }

    tab->documentView()->setDocumentInfo(fi.fileName(), fi.baseName());

    // Apply saved view mode from settings
    auto settingsViewMode = PrettyReaderSettings::self()->viewMode();
    DocumentView::ViewMode dvMode = DocumentView::Continuous;
    switch (settingsViewMode) {
    case PrettyReaderSettings::SinglePage:                dvMode = DocumentView::SinglePage; break;
    case PrettyReaderSettings::FacingPages:               dvMode = DocumentView::FacingPages; break;
    case PrettyReaderSettings::FacingPagesFirstAlone:     dvMode = DocumentView::FacingPagesFirstAlone; break;
    case PrettyReaderSettings::ContinuousFacing:          dvMode = DocumentView::ContinuousFacing; break;
    case PrettyReaderSettings::ContinuousFacingFirstAlone: dvMode = DocumentView::ContinuousFacingFirstAlone; break;
    default: break;
    }
    tab->documentView()->setViewMode(dvMode);

    // Connect zoom signal to status bar controls
    connect(tab->documentView(), &DocumentView::zoomChanged,
            this, [this](int percent) {
        if (m_zoomSpinBox) {
            const QSignalBlocker blocker(m_zoomSpinBox);
            m_zoomSpinBox->setValue(percent);
        }
        if (m_zoomSlider) {
            const QSignalBlocker blocker(m_zoomSlider);
            m_zoomSlider->setValue(percent);
        }
    });

    // A7: Connect hover hint signal to status bar
    connect(tab->documentView(), &DocumentView::statusHintChanged,
            this, [this](const QString &hint) {
        if (hint.isEmpty())
            statusBar()->clearMessage();
        else
            statusBar()->showMessage(hint);
    });

    // Code block language override: persist + rebuild
    connect(tab->documentView(), &DocumentView::codeBlockLanguageChanged,
            this, [this, filePath]() {
        auto *view = currentDocumentView();
        if (!view)
            return;
        // Persist overrides to MetadataStore as JSON
        QHash<QString, QString> overrides = view->codeBlockLanguageOverrides();
        QJsonObject langObj;
        for (auto it = overrides.cbegin(); it != overrides.cend(); ++it)
            langObj.insert(it.key(), it.value());
        m_metadataStore->setValue(filePath, QStringLiteral("codeBlockLanguages"), langObj);
        rebuildCurrentDocument();
    });

    int index = m_tabWidget->addTab(tab, fi.fileName());
    m_tabWidget->setTabToolTip(index, filePath);
    m_tabWidget->setCurrentIndex(index);

    // In web mode, build now that the tab is current (needs viewport width)
    if (webMode)
        rebuildCurrentDocument();

    m_recentFilesAction->addUrl(url);

    // Update TOC
    if (!tab->documentView()->isPdfMode()) {
        m_tocWidget->buildFromDocument(tab->documentView()->document());
    }
    // A6: Update file path label in status bar
    if (m_filePathLabel)
        m_filePathLabel->setText(filePath);

    statusBar()->showMessage(i18n("Opened %1", fi.fileName()), 3000);
}
