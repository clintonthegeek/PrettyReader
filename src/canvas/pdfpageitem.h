/*
 * pdfpageitem.h — Poppler-backed QGraphicsItem with async render
 *
 * Displays a single PDF page by requesting renders from RenderCache.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef PRETTYREADER_PDFPAGEITEM_H
#define PRETTYREADER_PDFPAGEITEM_H

#include <QGraphicsItem>
#include <QSizeF>

class RenderCache;

class PdfPageItem : public QGraphicsItem {
public:
    PdfPageItem(int pageNumber, QSizeF pageSize, RenderCache *cache,
                QGraphicsItem *parent = nullptr);

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
               QWidget *widget) override;

    void invalidateCache();
    void setPageNumber(int page);
    int pageNumber() const { return m_pageNumber; }
    QSizeF pageSize() const { return m_pageSize; }

    void setZoomFactor(qreal zoom);

private:
    int m_pageNumber;
    QSizeF m_pageSize; // in points
    RenderCache *m_cache;
    qreal m_zoom = 1.0;
};

#endif // PRETTYREADER_PDFPAGEITEM_H
