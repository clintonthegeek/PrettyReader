#include "pageitem.h"

#include <QAbstractTextDocumentLayout>
#include <QPainter>
#include <QStyleOptionGraphicsItem>

static constexpr qreal kShadowOffset = 4.0;

PageItem::PageItem(int pageNumber, QSizeF pageSize, QTextDocument *document,
                   QGraphicsItem *parent)
    : QGraphicsItem(parent)
    , m_pageNumber(pageNumber)
    , m_pageSize(pageSize)
    , m_document(document)
{
}

QRectF PageItem::boundingRect() const
{
    return QRectF(QPointF(0, 0), m_pageSize);
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

    // Render the document content for this page
    // QTextDocument::setPageSize() causes it to paginate; each page occupies
    // a vertical slice of height pageSize().height().
    qreal pageHeight = m_pageSize.height();

    // Use documentLayout()->draw() with explicit palette to avoid
    // dark-theme QPalette bleeding light text onto white pages
    painter->save();
    painter->translate(0, -m_pageNumber * pageHeight);

    QAbstractTextDocumentLayout::PaintContext ctx;
    ctx.clip = QRectF(0, m_pageNumber * pageHeight,
                      m_pageSize.width(), pageHeight);
    QPalette pal;
    pal.setColor(QPalette::Text, QColor(0x1a, 0x1a, 0x1a));
    pal.setColor(QPalette::Base, Qt::white);
    ctx.palette = pal;
    m_document->documentLayout()->draw(painter, ctx);
    painter->restore();
}
