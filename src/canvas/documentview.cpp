#include "documentview.h"
#include "pageitem.h"
#include "pdfpageitem.h"
#include "rendercache.h"
#include "pagelayout.h"

#include <QGraphicsScene>
#include <QMouseEvent>
#include <QScrollBar>
#include <QTimer>
#include <QWheelEvent>
#include <QtMath>

#include <poppler-qt6.h>

DocumentView::DocumentView(QWidget *parent)
    : QGraphicsView(parent)
{
    m_scene = new QGraphicsScene(this);
    setScene(m_scene);

    setBackgroundBrush(QBrush(QColor(0x3c, 0x3c, 0x3c)));
    setRenderHint(QPainter::Antialiasing);
    setRenderHint(QPainter::TextAntialiasing);
    setDragMode(QGraphicsView::ScrollHandDrag);
    setViewportUpdateMode(QGraphicsView::MinimalViewportUpdate);

    // Default A4 page size in points (595 x 842)
    m_pageSize = QSizeF(595, 842);

    // Create render cache for PDF mode
    m_renderCache = new RenderCache(this);
    connect(m_renderCache, &RenderCache::pixmapReady,
            this, &DocumentView::onPixmapReady);
}

DocumentView::~DocumentView()
{
    clearPdfPages();
    delete m_popplerDoc;
}

// --- Legacy QTextDocument path ---

void DocumentView::setDocument(QTextDocument *doc)
{
    clearPdfPages();
    m_pdfMode = false;

    qDeleteAll(m_pageItems);
    m_pageItems.clear();
    m_scene->clear();

    m_document = doc;
    if (!m_document) {
        m_pageCount = 0;
        return;
    }

    qreal dpi = logicalDpiX();
    qreal s = (dpi > 0) ? dpi / 72.0 : 1.0;
    QSizeF contentPts = m_marginsPoints.isNull()
        ? m_pageSize
        : m_pageLayout.contentSizePoints();
    m_document->setPageSize(QSizeF(contentPts.width() * s,
                                   contentPts.height() * s));
    m_pageCount = m_document->pageCount();

    layoutPages();

    if (!m_pageItems.isEmpty()) {
        m_currentPage = 0;
        if (!m_skipAutoFit) {
            QTimer::singleShot(0, this, &DocumentView::fitWidth);
        }
        m_skipAutoFit = false;
    }
}

// --- New PDF path ---

void DocumentView::setPdfData(const QByteArray &pdf)
{
    // Clean up legacy path
    qDeleteAll(m_pageItems);
    m_pageItems.clear();
    m_document = nullptr;

    // Clean up old PDF
    clearPdfPages();
    m_scene->clear();

    m_pdfData = pdf;
    m_pdfMode = true;

    // Load via Poppler
    delete m_popplerDoc;
    m_popplerDoc = Poppler::Document::loadFromData(pdf).release();
    if (!m_popplerDoc) {
        m_pageCount = 0;
        return;
    }
    m_popplerDoc->setRenderHint(Poppler::Document::Antialiasing, true);
    m_popplerDoc->setRenderHint(Poppler::Document::TextAntialiasing, true);

    m_renderCache->setDocument(m_popplerDoc);
    m_pageCount = m_popplerDoc->numPages();

    layoutPages();

    if (!m_pdfPageItems.isEmpty()) {
        m_currentPage = 0;
        if (!m_skipAutoFit) {
            QTimer::singleShot(0, this, &DocumentView::fitWidth);
        }
        m_skipAutoFit = false;
    }
}

void DocumentView::clearPdfPages()
{
    for (auto *item : m_pdfPageItems) {
        m_scene->removeItem(item);
        delete item;
    }
    m_pdfPageItems.clear();
}

void DocumentView::onPixmapReady(int pageNumber)
{
    // Trigger repaint for the affected page item
    for (auto *item : m_pdfPageItems) {
        if (item->pageNumber() == pageNumber) {
            item->update();
            break;
        }
    }
}

// --- Common ---

void DocumentView::setDocumentInfo(const QString &fileName, const QString &title)
{
    m_fileName = fileName;
    m_title = title;

    if (!m_pdfMode) {
        for (auto *page : m_pageItems)
            page->setDocumentInfo(m_pageCount, m_fileName, m_title);
    }

    if (m_scene)
        m_scene->update();
}

ViewState DocumentView::saveViewState() const
{
    ViewState state;
    state.zoomPercent = m_currentZoom;
    state.currentPage = m_currentPage;
    auto *vbar = verticalScrollBar();
    if (vbar && vbar->maximum() > 0)
        state.scrollFraction = static_cast<qreal>(vbar->value()) / vbar->maximum();
    else
        state.scrollFraction = 0.0;
    state.valid = true;
    return state;
}

void DocumentView::restoreViewState(const ViewState &state)
{
    if (!state.valid)
        return;

    m_skipAutoFit = true;
    setZoomPercent(state.zoomPercent);

    QTimer::singleShot(0, this, [this, state]() {
        auto *vbar = verticalScrollBar();
        if (vbar && vbar->maximum() > 0) {
            vbar->setValue(qRound(state.scrollFraction * vbar->maximum()));
        }
        m_currentPage = qBound(0, state.currentPage, m_pageCount - 1);
        Q_EMIT currentPageChanged(m_currentPage);
    });
}

// --- Layout ---

void DocumentView::layoutPages()
{
    if (m_pdfMode)
    {
        switch (m_viewMode) {
        case Continuous:                    layoutPagesContinuous(); break;
        case SinglePage:                    layoutPagesSingle(); break;
        case FacingPages:                   layoutPagesFacing(); break;
        case FacingPagesFirstAlone:         layoutPagesFacingFirstAlone(); break;
        case ContinuousFacing:              layoutPagesContinuousFacing(); break;
        case ContinuousFacingFirstAlone:    layoutPagesContinuousFacingFirstAlone(); break;
        }
        return;
    }

    // Legacy QTextDocument continuous layout
    layoutPagesContinuous();
}

void DocumentView::layoutPagesContinuous()
{
    if (m_pdfMode) {
        clearPdfPages();
        qreal yOffset = kPageGap;
        qreal sceneWidth = m_pageSize.width() + kSceneMargin * 2;

        for (int i = 0; i < m_pageCount; ++i) {
            QSizeF pageSize = m_pageSize;
            if (m_popplerDoc) {
                std::unique_ptr<Poppler::Page> pp(m_popplerDoc->page(i));
                if (pp)
                    pageSize = pp->pageSizeF();
            }

            auto *item = new PdfPageItem(i, pageSize, m_renderCache);
            item->setZoomFactor(m_currentZoom / 100.0);
            qreal xOffset = (sceneWidth - pageSize.width()) / 2.0;
            item->setPos(xOffset, yOffset);
            m_scene->addItem(item);
            m_pdfPageItems.append(item);

            yOffset += pageSize.height() + kPageGap;
        }

        m_scene->setSceneRect(0, 0, sceneWidth, yOffset + kPageGap);
        return;
    }

    // Legacy path
    qreal yOffset = kPageGap;
    qreal sceneWidth = m_pageSize.width() + kSceneMargin * 2;

    for (int i = 0; i < m_pageCount; ++i) {
        PageItem *page;
        if (i < m_pageItems.size()) {
            page = m_pageItems[i];
            page->setPageNumber(i);
            page->setPageLayout(m_pageLayout);
            page->setDocumentInfo(m_pageCount, m_fileName, m_title);
        } else {
            page = new PageItem(i, m_pageSize, m_document, m_marginsPoints);
            page->setPageLayout(m_pageLayout);
            page->setDocumentInfo(m_pageCount, m_fileName, m_title);
            page->setFlag(QGraphicsItem::ItemUsesExtendedStyleOption);
            m_scene->addItem(page);
            m_pageItems.append(page);
        }

        qreal xOffset = (sceneWidth - m_pageSize.width()) / 2.0;
        page->setPos(xOffset, yOffset);
        yOffset += m_pageSize.height() + kPageGap;
    }

    while (m_pageItems.size() > m_pageCount) {
        auto *item = m_pageItems.takeLast();
        m_scene->removeItem(item);
        delete item;
    }

    m_scene->setSceneRect(0, 0, sceneWidth, yOffset + kPageGap);
}

void DocumentView::layoutPagesSingle()
{
    if (!m_pdfMode) return;
    clearPdfPages();

    if (m_pageCount == 0) return;
    int page = qBound(0, m_currentPage, m_pageCount - 1);

    QSizeF pageSize = m_pageSize;
    if (m_popplerDoc) {
        std::unique_ptr<Poppler::Page> pp(m_popplerDoc->page(page));
        if (pp) pageSize = pp->pageSizeF();
    }

    auto *item = new PdfPageItem(page, pageSize, m_renderCache);
    item->setZoomFactor(m_currentZoom / 100.0);
    item->setPos(kSceneMargin, kPageGap);
    m_scene->addItem(item);
    m_pdfPageItems.append(item);

    m_scene->setSceneRect(0, 0,
                          pageSize.width() + kSceneMargin * 2,
                          pageSize.height() + kPageGap * 2);
}

void DocumentView::layoutPagesFacing()
{
    if (!m_pdfMode) return;
    clearPdfPages();

    if (m_pageCount == 0) return;

    // Show current page and its facing partner
    int leftPage = (m_currentPage / 2) * 2;
    int rightPage = leftPage + 1;

    qreal maxHeight = 0;
    qreal totalWidth = 0;

    for (int p : {leftPage, rightPage}) {
        if (p >= m_pageCount) continue;
        QSizeF pageSize = m_pageSize;
        if (m_popplerDoc) {
            std::unique_ptr<Poppler::Page> pp(m_popplerDoc->page(p));
            if (pp) pageSize = pp->pageSizeF();
        }

        auto *item = new PdfPageItem(p, pageSize, m_renderCache);
        item->setZoomFactor(m_currentZoom / 100.0);
        item->setPos(kSceneMargin + totalWidth, kPageGap);
        m_scene->addItem(item);
        m_pdfPageItems.append(item);

        totalWidth += pageSize.width() + kPageGap;
        maxHeight = qMax(maxHeight, pageSize.height());
    }

    m_scene->setSceneRect(0, 0,
                          totalWidth + kSceneMargin * 2,
                          maxHeight + kPageGap * 2);
}

void DocumentView::layoutPagesContinuousFacing()
{
    if (!m_pdfMode) return;
    clearPdfPages();

    qreal yOffset = kPageGap;
    qreal maxWidth = m_pageSize.width() * 2 + kPageGap + kSceneMargin * 2;

    for (int i = 0; i < m_pageCount; i += 2) {
        qreal rowHeight = 0;
        qreal xOffset = kSceneMargin;

        for (int j = 0; j < 2 && (i + j) < m_pageCount; ++j) {
            int page = i + j;
            QSizeF pageSize = m_pageSize;
            if (m_popplerDoc) {
                std::unique_ptr<Poppler::Page> pp(m_popplerDoc->page(page));
                if (pp) pageSize = pp->pageSizeF();
            }

            auto *item = new PdfPageItem(page, pageSize, m_renderCache);
            item->setZoomFactor(m_currentZoom / 100.0);
            item->setPos(xOffset, yOffset);
            m_scene->addItem(item);
            m_pdfPageItems.append(item);

            xOffset += pageSize.width() + kPageGap;
            rowHeight = qMax(rowHeight, pageSize.height());
        }

        yOffset += rowHeight + kPageGap;
    }

    m_scene->setSceneRect(0, 0, maxWidth, yOffset + kPageGap);
}

void DocumentView::layoutPagesFacingFirstAlone()
{
    if (!m_pdfMode) return;
    clearPdfPages();

    if (m_pageCount == 0) return;

    // First page standalone: page 0 alone, then 1-2, 3-4, etc.
    // Determine which spread contains the current page
    int spreadStart;
    if (m_currentPage == 0) {
        spreadStart = 0;
    } else {
        // Pages 1-2 = spread 1, 3-4 = spread 2, etc.
        spreadStart = 1 + ((m_currentPage - 1) / 2) * 2;
    }

    qreal maxHeight = 0;
    qreal totalWidth = 0;
    int spreadEnd = (spreadStart == 0) ? 1 : spreadStart + 2;

    for (int p = spreadStart; p < spreadEnd && p < m_pageCount; ++p) {
        QSizeF pageSize = m_pageSize;
        if (m_popplerDoc) {
            std::unique_ptr<Poppler::Page> pp(m_popplerDoc->page(p));
            if (pp) pageSize = pp->pageSizeF();
        }

        auto *item = new PdfPageItem(p, pageSize, m_renderCache);
        item->setZoomFactor(m_currentZoom / 100.0);
        item->setPos(kSceneMargin + totalWidth, kPageGap);
        m_scene->addItem(item);
        m_pdfPageItems.append(item);

        totalWidth += pageSize.width() + kPageGap;
        maxHeight = qMax(maxHeight, pageSize.height());
    }

    m_scene->setSceneRect(0, 0,
                          totalWidth + kSceneMargin * 2,
                          maxHeight + kPageGap * 2);
}

void DocumentView::layoutPagesContinuousFacingFirstAlone()
{
    if (!m_pdfMode) return;
    clearPdfPages();

    qreal yOffset = kPageGap;
    qreal maxWidth = m_pageSize.width() * 2 + kPageGap + kSceneMargin * 2;

    // Page 0 is alone (centered), then 1-2, 3-4, etc.
    if (m_pageCount > 0) {
        QSizeF pageSize = m_pageSize;
        if (m_popplerDoc) {
            std::unique_ptr<Poppler::Page> pp(m_popplerDoc->page(0));
            if (pp) pageSize = pp->pageSizeF();
        }

        auto *item = new PdfPageItem(0, pageSize, m_renderCache);
        item->setZoomFactor(m_currentZoom / 100.0);
        // Center the first page (offset to the right half of the spread)
        qreal xOffset = kSceneMargin + m_pageSize.width() + kPageGap;
        xOffset = (maxWidth - pageSize.width()) / 2.0;
        item->setPos(xOffset, yOffset);
        m_scene->addItem(item);
        m_pdfPageItems.append(item);

        yOffset += pageSize.height() + kPageGap;
    }

    // Remaining pages in pairs: 1-2, 3-4, 5-6, ...
    for (int i = 1; i < m_pageCount; i += 2) {
        qreal rowHeight = 0;
        qreal xOffset = kSceneMargin;

        for (int j = 0; j < 2 && (i + j) < m_pageCount; ++j) {
            int page = i + j;
            QSizeF pageSize = m_pageSize;
            if (m_popplerDoc) {
                std::unique_ptr<Poppler::Page> pp(m_popplerDoc->page(page));
                if (pp) pageSize = pp->pageSizeF();
            }

            auto *item = new PdfPageItem(page, pageSize, m_renderCache);
            item->setZoomFactor(m_currentZoom / 100.0);
            item->setPos(xOffset, yOffset);
            m_scene->addItem(item);
            m_pdfPageItems.append(item);

            xOffset += pageSize.width() + kPageGap;
            rowHeight = qMax(rowHeight, pageSize.height());
        }

        yOffset += rowHeight + kPageGap;
    }

    m_scene->setSceneRect(0, 0, maxWidth, yOffset + kPageGap);
}

// --- Page size/layout ---

void DocumentView::setPageSize(const QSizeF &size)
{
    if (size == m_pageSize)
        return;
    m_pageSize = size;
    m_marginsPoints = QMarginsF();
    if (m_pdfMode) {
        layoutPages();
    } else if (m_document) {
        qreal dpi = logicalDpiX();
        qreal s = (dpi > 0) ? dpi / 72.0 : 1.0;
        m_document->setPageSize(QSizeF(m_pageSize.width() * s,
                                       m_pageSize.height() * s));
        m_pageCount = m_document->pageCount();
        layoutPages();
    }
}

void DocumentView::setPageLayout(const PageLayout &layout)
{
    m_pageLayout = layout;
    m_pageSize = layout.pageSizePoints();
    m_marginsPoints = layout.marginsPoints();

    if (m_pdfMode) {
        layoutPages();
    } else if (m_document) {
        qreal dpi = logicalDpiX();
        qreal s = (dpi > 0) ? dpi / 72.0 : 1.0;
        QSizeF contentPts = layout.contentSizePoints();
        m_document->setPageSize(QSizeF(contentPts.width() * s,
                                       contentPts.height() * s));
        m_pageCount = m_document->pageCount();
        layoutPages();
    }
}

// --- Zoom ---

void DocumentView::setZoomPercent(int percent)
{
    percent = qBound(25, percent, 400);
    qreal factor = percent / 100.0;
    resetTransform();
    scale(factor, factor);
    m_currentZoom = percent;

    // Update PDF page items zoom
    for (auto *item : m_pdfPageItems)
        item->setZoomFactor(factor);

    Q_EMIT zoomChanged(percent);
}

void DocumentView::zoomIn()
{
    setZoomPercent(m_currentZoom + 10);
}

void DocumentView::zoomOut()
{
    setZoomPercent(m_currentZoom - 10);
}

void DocumentView::fitWidth()
{
    qreal pageWidth = m_pageSize.width();
    if (!m_pageItems.isEmpty())
        pageWidth = m_pageItems.first()->boundingRect().width();
    else if (!m_pdfPageItems.isEmpty())
        pageWidth = m_pdfPageItems.first()->boundingRect().width();
    else
        return;

    qreal viewWidth = viewport()->width();
    qreal factor = viewWidth / (pageWidth + kSceneMargin);
    resetTransform();
    scale(factor, factor);
    m_currentZoom = qRound(factor * 100);

    for (auto *item : m_pdfPageItems)
        item->setZoomFactor(factor);

    Q_EMIT zoomChanged(m_currentZoom);
}

void DocumentView::fitPage()
{
    QRectF pageRect;
    if (!m_pageItems.isEmpty())
        pageRect = m_pageItems.first()->boundingRect();
    else if (!m_pdfPageItems.isEmpty())
        pageRect = m_pdfPageItems.first()->boundingRect();
    else
        return;

    pageRect.adjust(-20, -20, 20, 20);
    fitInView(pageRect, Qt::KeepAspectRatio);
    m_currentZoom = qRound(transform().m11() * 100);

    for (auto *item : m_pdfPageItems)
        item->setZoomFactor(transform().m11());

    Q_EMIT zoomChanged(m_currentZoom);
}

// --- Navigation ---

void DocumentView::goToPage(int page)
{
    if (page < 0 || page >= m_pageCount)
        return;
    m_currentPage = page;

    if (m_pdfMode) {
        if (m_viewMode == SinglePage || m_viewMode == FacingPages) {
            layoutPages(); // relayout to show new page
        } else {
            // Continuous: scroll to page
            if (page < m_pdfPageItems.size())
                ensureVisible(m_pdfPageItems[page]);
        }
    } else {
        if (m_viewMode == SinglePage) {
            if (page < m_pageItems.size()) {
                QRectF r = m_pageItems[page]->boundingRect()
                               .translated(m_pageItems[page]->pos());
                r.adjust(-20, -20, 20, 20);
                fitInView(r, Qt::KeepAspectRatio);
                m_currentZoom = qRound(transform().m11() * 100);
                Q_EMIT zoomChanged(m_currentZoom);
            }
        } else {
            if (page < m_pageItems.size())
                ensureVisible(m_pageItems[page]);
        }
    }
    Q_EMIT currentPageChanged(page);
}

void DocumentView::previousPage()
{
    if (m_currentPage > 0)
        goToPage(m_currentPage - 1);
}

void DocumentView::nextPage()
{
    if (m_currentPage < m_pageCount - 1)
        goToPage(m_currentPage + 1);
}

// --- View mode ---

void DocumentView::setViewMode(ViewMode mode)
{
    if (m_viewMode == mode)
        return;
    m_viewMode = mode;

    bool continuous = (mode == Continuous || mode == ContinuousFacing || mode == ContinuousFacingFirstAlone);
    setVerticalScrollBarPolicy(continuous ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);

    layoutPages();
    Q_EMIT viewModeChanged(mode);
}

void DocumentView::setContinuousMode(bool continuous)
{
    setViewMode(continuous ? Continuous : SinglePage);
}

// --- Events ---

void DocumentView::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier) {
        int delta = event->angleDelta().y() > 0 ? 10 : -10;
        setZoomPercent(m_currentZoom + delta);
        centerOn(mapToScene(event->position().toPoint()));
        event->accept();
    } else {
        QGraphicsView::wheelEvent(event);
    }
}

void DocumentView::resizeEvent(QResizeEvent *event)
{
    QGraphicsView::resizeEvent(event);
}

void DocumentView::scrollContentsBy(int dx, int dy)
{
    QGraphicsView::scrollContentsBy(dx, dy);
    updateCurrentPage();
}

// --- Middle-mouse smooth zoom (Okular pattern) ---
// Middle-click + drag up → zoom in, drag down → zoom out, centered on click point.

void DocumentView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MiddleButton) {
        m_middleZooming = true;
        m_middleZoomOrigin = event->pos();
        m_middleZoomSceneAnchor = mapToScene(event->pos());
        m_middleZoomStartPercent = m_currentZoom;
        setCursor(Qt::SizeVerCursor);
        event->accept();
    } else {
        QGraphicsView::mousePressEvent(event);
    }
}

void DocumentView::mouseMoveEvent(QMouseEvent *event)
{
    if (m_middleZooming) {
        // Vertical delta: drag up = positive = zoom in, drag down = negative = zoom out
        int dy = m_middleZoomOrigin.y() - event->pos().y();
        // 200 pixels of drag = double/halve the zoom
        qreal sensitivity = 200.0;
        qreal factor = qPow(2.0, dy / sensitivity);
        int newZoom = qBound(25, qRound(m_middleZoomStartPercent * factor), 400);

        if (newZoom != m_currentZoom) {
            // Only scale the view transform — bitmap stretch, no Poppler re-render.
            // PdfPageItems keep their old zoom factor and cached pixmaps.
            // Crisp re-render happens on mouse release.
            qreal scaleFactor = newZoom / 100.0;
            resetTransform();
            scale(scaleFactor, scaleFactor);
            m_currentZoom = newZoom;

            centerOn(m_middleZoomSceneAnchor);
            Q_EMIT zoomChanged(m_currentZoom);
        }
        event->accept();
    } else {
        QGraphicsView::mouseMoveEvent(event);
    }
}

void DocumentView::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MiddleButton && m_middleZooming) {
        m_middleZooming = false;
        setCursor(Qt::ArrowCursor);

        // Now trigger crisp Poppler re-render at the final zoom level
        qreal scaleFactor = m_currentZoom / 100.0;
        for (auto *item : m_pdfPageItems)
            item->setZoomFactor(scaleFactor);

        event->accept();
    } else {
        QGraphicsView::mouseReleaseEvent(event);
    }
}

void DocumentView::updateCurrentPage()
{
    QPointF center = mapToScene(viewport()->rect().center());

    // Check PDF pages
    for (int i = 0; i < m_pdfPageItems.size(); ++i) {
        QRectF r = m_pdfPageItems[i]->boundingRect().translated(m_pdfPageItems[i]->pos());
        if (r.contains(center)) {
            int page = m_pdfPageItems[i]->pageNumber();
            if (m_currentPage != page) {
                m_currentPage = page;
                Q_EMIT currentPageChanged(page);
            }
            return;
        }
    }

    // Check legacy pages
    for (int i = 0; i < m_pageItems.size(); ++i) {
        QRectF r = m_pageItems[i]->boundingRect().translated(m_pageItems[i]->pos());
        if (r.contains(center)) {
            if (m_currentPage != i) {
                m_currentPage = i;
                Q_EMIT currentPageChanged(i);
            }
            return;
        }
    }
}
