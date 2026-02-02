#ifndef PRETTYREADER_PAGELAYOUT_H
#define PRETTYREADER_PAGELAYOUT_H

#include <QMarginsF>
#include <QPageLayout>
#include <QPageSize>
#include <QSizeF>
#include <QString>

struct PageLayout
{
    QPageSize::PageSizeId pageSizeId = QPageSize::A4;
    QPageLayout::Orientation orientation = QPageLayout::Portrait;
    QMarginsF margins{25.0, 25.0, 25.0, 25.0}; // mm

    // Header/footer configuration
    bool headerEnabled = false;
    bool footerEnabled = true;
    QString headerLeft;
    QString headerCenter;
    QString headerRight;
    QString footerLeft;
    QString footerCenter;
    QString footerRight{QStringLiteral("{page} / {pages}")};

    // Height constants (points)
    static constexpr qreal kHeaderHeight = 16.0;
    static constexpr qreal kFooterHeight = 14.0;
    static constexpr qreal kSeparatorGap = 6.0;

    qreal headerTotalHeight() const
    {
        return headerEnabled ? (kHeaderHeight + kSeparatorGap) : 0.0;
    }

    qreal footerTotalHeight() const
    {
        return footerEnabled ? (kFooterHeight + kSeparatorGap) : 0.0;
    }

    // Return the content area size in points (72 dpi)
    // Subtracts margins and header/footer heights from the full page size.
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
                      full.height() - top - bottom
                          - headerTotalHeight() - footerTotalHeight());
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

    // Return margins in points
    QMarginsF marginsPoints() const
    {
        constexpr qreal mmToPt = 72.0 / 25.4;
        return QMarginsF(margins.left()   * mmToPt,
                         margins.top()    * mmToPt,
                         margins.right()  * mmToPt,
                         margins.bottom() * mmToPt);
    }
};

#endif // PRETTYREADER_PAGELAYOUT_H
