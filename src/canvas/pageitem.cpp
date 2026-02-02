#include "pageitem.h"
#include "headerfooterrenderer.h"

#include <QAbstractTextDocumentLayout>
#include <QPainter>
#include <QStyleOptionGraphicsItem>

static constexpr qreal kShadowOffset = 4.0;

PageItem::PageItem(int pageNumber, QSizeF pageSize, QTextDocument *document,
                   const QMarginsF &margins, QGraphicsItem *parent)
    : QGraphicsItem(parent)
    , m_pageNumber(pageNumber)
    , m_pageSize(pageSize)
    , m_document(document)
    , m_margins(margins)
{
}

QRectF PageItem::boundingRect() const
{
    return QRectF(QPointF(0, 0), m_pageSize);
}

void PageItem::setPageLayout(const PageLayout &layout)
{
    m_pageLayout = layout;
    m_margins = layout.marginsPoints();
}

void PageItem::setDocumentInfo(int totalPages, const QString &fileName,
                               const QString &title)
{
    m_totalPages = totalPages;
    m_fileName = fileName;
    m_title = title;
}

void PageItem::paint(QPainter *painter,
                     const QStyleOptionGraphicsItem *option, QWidget *)
{
    QRectF exposed = option->exposedRect;
    painter->setClipRect(exposed);

    // Drop shadow
    painter->fillRect(boundingRect().translated(kShadowOffset, kShadowOffset),
                      QColor(0, 0, 0, 50));

    // White page background
    painter->fillRect(boundingRect(), Qt::white);

    // Page border
    painter->setPen(QPen(Qt::lightGray, 0.5));
    painter->drawRect(boundingRect());

    // Calculate margin area
    qreal contentX = m_margins.left();
    qreal contentY = m_margins.top();
    qreal contentW = m_pageSize.width() - m_margins.left() - m_margins.right();
    qreal contentH = m_pageSize.height() - m_margins.top() - m_margins.bottom();

    // If no margins, use full page (backward-compatible)
    if (m_margins.isNull()) {
        contentX = 0;
        contentY = 0;
        contentW = m_pageSize.width();
        contentH = m_pageSize.height();
    }

    // Compute header/body/footer rects within the margin area
    qreal headerH = m_pageLayout.headerTotalHeight();
    qreal footerH = m_pageLayout.footerTotalHeight();
    qreal bodyH = contentH - headerH - footerH;

    QRectF headerRect(contentX, contentY,
                      contentW, PageLayout::kHeaderHeight);
    QRectF bodyRect(contentX, contentY + headerH,
                    contentW, bodyH);
    QRectF footerRect(contentX, contentY + contentH - PageLayout::kFooterHeight,
                      contentW, PageLayout::kFooterHeight);

    // Build metadata for this page
    PageMetadata meta;
    meta.pageNumber = m_pageNumber;
    meta.totalPages = m_totalPages;
    meta.fileName = m_fileName;
    meta.title = m_title;

    // Draw header
    HeaderFooterRenderer::drawHeader(painter, headerRect, m_pageLayout, meta, 0.5);

    // Render the document content for this page, clipped to body area
    painter->save();
    painter->setClipRect(bodyRect);
    painter->translate(bodyRect.topLeft());
    painter->translate(0, -m_pageNumber * bodyH);

    QAbstractTextDocumentLayout::PaintContext ctx;
    ctx.clip = QRectF(0, m_pageNumber * bodyH,
                      contentW, bodyH);
    QPalette pal;
    pal.setColor(QPalette::Text, QColor(0x1a, 0x1a, 0x1a));
    pal.setColor(QPalette::Base, Qt::white);
    ctx.palette = pal;
    m_document->documentLayout()->draw(painter, ctx);
    painter->restore();

    // Draw footer
    HeaderFooterRenderer::drawFooter(painter, footerRect, m_pageLayout, meta, 0.5);
}
