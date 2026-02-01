#ifndef PRETTYREADER_PAGELAYOUT_H
#define PRETTYREADER_PAGELAYOUT_H

#include <QMarginsF>
#include <QPageLayout>
#include <QPageSize>
#include <QSizeF>

struct PageLayout
{
    QPageSize::PageSizeId pageSizeId = QPageSize::A4;
    QPageLayout::Orientation orientation = QPageLayout::Portrait;
    QMarginsF margins{25.0, 25.0, 25.0, 25.0}; // mm

    // Return the content area size in points (72 dpi)
    QSizeF contentSizePoints() const
    {
        QPageSize ps(pageSizeId);
        QSizeF full = ps.size(QPageSize::Point);
        if (orientation == QPageLayout::Landscape)
            full.transpose();

        // Convert mm margins to points (1mm = 2.8346pt)
        constexpr qreal mmToPt = 72.0 / 25.4;
        qreal left   = margins.left()   * mmToPt;
        qreal right  = margins.right()  * mmToPt;
        qreal top    = margins.top()    * mmToPt;
        qreal bottom = margins.bottom() * mmToPt;

        return QSizeF(full.width() - left - right,
                      full.height() - top - bottom);
    }

    // Return the full page size in points
    QSizeF pageSizePoints() const
    {
        QPageSize ps(pageSizeId);
        QSizeF full = ps.size(QPageSize::Point);
        if (orientation == QPageLayout::Landscape)
            full.transpose();
        return full;
    }
};

#endif // PRETTYREADER_PAGELAYOUT_H
