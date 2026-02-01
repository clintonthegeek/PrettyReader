#ifndef PRETTYREADER_PAGEITEM_H
#define PRETTYREADER_PAGEITEM_H

#include <QGraphicsItem>
#include <QTextDocument>

class PageItem : public QGraphicsItem
{
public:
    PageItem(int pageNumber, QSizeF pageSize, QTextDocument *document,
             QGraphicsItem *parent = nullptr);

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
               QWidget *widget) override;

    int pageNumber() const { return m_pageNumber; }
    void setPageNumber(int page) { m_pageNumber = page; }

private:
    int m_pageNumber;
    QSizeF m_pageSize;
    QTextDocument *m_document;
};

#endif // PRETTYREADER_PAGEITEM_H
