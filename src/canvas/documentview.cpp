#include "documentview.h"
#include "pageitem.h"

#include <QGraphicsScene>
#include <QScrollBar>
#include <QTimer>
#include <QWheelEvent>

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
}

void DocumentView::setDocument(QTextDocument *doc)
{
    // Clean up old pages
    qDeleteAll(m_pageItems);
    m_pageItems.clear();
    m_scene->clear();

    m_document = doc;
    if (!m_document) {
        m_pageCount = 0;
        return;
    }

    // Set page size so QTextDocument paginates
    m_document->setPageSize(m_pageSize);
    m_pageCount = m_document->pageCount();

    layoutPages();

    // Start at first page; defer fitWidth until layout is complete
    if (!m_pageItems.isEmpty()) {
        m_currentPage = 0;
        QTimer::singleShot(0, this, &DocumentView::fitWidth);
    }
}

void DocumentView::layoutPages()
{
    qreal yOffset = kPageGap;
    qreal sceneWidth = m_pageSize.width() + kSceneMargin * 2;

    for (int i = 0; i < m_pageCount; ++i) {
        PageItem *page;
        if (i < m_pageItems.size()) {
            page = m_pageItems[i];
            page->setPageNumber(i);
        } else {
            page = new PageItem(i, m_pageSize, m_document);
            page->setFlag(QGraphicsItem::ItemUsesExtendedStyleOption);
            m_scene->addItem(page);
            m_pageItems.append(page);
        }

        qreal xOffset = (sceneWidth - m_pageSize.width()) / 2.0;
        page->setPos(xOffset, yOffset);
        yOffset += m_pageSize.height() + kPageGap;
    }

    // Remove excess items
    while (m_pageItems.size() > m_pageCount) {
        auto *item = m_pageItems.takeLast();
        m_scene->removeItem(item);
        delete item;
    }

    m_scene->setSceneRect(0, 0, sceneWidth, yOffset + kPageGap);
}

void DocumentView::setPageSize(const QSizeF &size)
{
    if (size == m_pageSize)
        return;
    m_pageSize = size;
    if (m_document) {
        m_document->setPageSize(m_pageSize);
        m_pageCount = m_document->pageCount();
        layoutPages();
    }
}

void DocumentView::setZoomPercent(int percent)
{
    percent = qBound(25, percent, 400);
    qreal factor = percent / 100.0;
    resetTransform();
    scale(factor, factor);
    m_currentZoom = percent;
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
    if (m_pageItems.isEmpty())
        return;

    qreal pageWidth = m_pageItems.first()->boundingRect().width();
    qreal viewWidth = viewport()->width();
    qreal factor = viewWidth / (pageWidth + kSceneMargin);
    resetTransform();
    scale(factor, factor);
    m_currentZoom = qRound(factor * 100);
    Q_EMIT zoomChanged(m_currentZoom);
}

void DocumentView::fitPage()
{
    if (m_pageItems.isEmpty())
        return;

    QRectF pageRect = m_pageItems.first()->boundingRect();
    pageRect.adjust(-20, -20, 20, 20);
    fitInView(pageRect, Qt::KeepAspectRatio);
    m_currentZoom = qRound(transform().m11() * 100);
    Q_EMIT zoomChanged(m_currentZoom);
}

void DocumentView::goToPage(int page)
{
    if (page < 0 || page >= m_pageItems.size())
        return;
    m_currentPage = page;

    if (!m_continuousMode) {
        QRectF pageRect = m_pageItems[page]->boundingRect()
                              .translated(m_pageItems[page]->pos());
        pageRect.adjust(-20, -20, 20, 20);
        fitInView(pageRect, Qt::KeepAspectRatio);
        m_currentZoom = qRound(transform().m11() * 100);
        Q_EMIT zoomChanged(m_currentZoom);
    } else {
        ensureVisible(m_pageItems[page]);
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

void DocumentView::setContinuousMode(bool continuous)
{
    m_continuousMode = continuous;
    if (continuous) {
        setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    } else {
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        goToPage(m_currentPage);
    }
}

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

void DocumentView::updateCurrentPage()
{
    if (m_pageItems.isEmpty())
        return;

    QPointF center = mapToScene(viewport()->rect().center());
    for (int i = 0; i < m_pageItems.size(); ++i) {
        QRectF pageRect = m_pageItems[i]->boundingRect()
                              .translated(m_pageItems[i]->pos());
        if (pageRect.contains(center)) {
            if (m_currentPage != i) {
                m_currentPage = i;
                Q_EMIT currentPageChanged(i);
            }
            return;
        }
    }
}
