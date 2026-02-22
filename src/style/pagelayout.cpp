/*
 * pagelayout.cpp — JSON serialization for PageLayout
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "pagelayout.h"

#include <QJsonObject>

// ---------------------------------------------------------------------------
// fromJson / toJson for page layout
// ---------------------------------------------------------------------------

PageLayout PageLayout::fromJson(const QJsonObject &plObj, const QJsonObject &mpObj)
{
    PageLayout pl;

    if (plObj.contains(QLatin1String("pageSize"))) {
        QString sizeStr = plObj.value(QLatin1String("pageSize")).toString();
        if (sizeStr == QLatin1String("Letter"))      pl.pageSizeId = QPageSize::Letter;
        else if (sizeStr == QLatin1String("A5"))      pl.pageSizeId = QPageSize::A5;
        else if (sizeStr == QLatin1String("Legal"))   pl.pageSizeId = QPageSize::Legal;
        else if (sizeStr == QLatin1String("B5"))      pl.pageSizeId = QPageSize::B5;
        else                                           pl.pageSizeId = QPageSize::A4;
    }
    if (plObj.contains(QLatin1String("orientation"))) {
        QString orient = plObj.value(QLatin1String("orientation")).toString();
        pl.orientation = (orient == QLatin1String("landscape"))
            ? QPageLayout::Landscape : QPageLayout::Portrait;
    }
    if (plObj.contains(QLatin1String("margins"))) {
        QJsonObject m = plObj.value(QLatin1String("margins")).toObject();
        pl.margins = QMarginsF(
            m.value(QLatin1String("left")).toDouble(25.0),
            m.value(QLatin1String("top")).toDouble(25.0),
            m.value(QLatin1String("right")).toDouble(25.0),
            m.value(QLatin1String("bottom")).toDouble(25.0));
    }
    if (plObj.contains(QLatin1String("header"))) {
        QJsonObject h = plObj.value(QLatin1String("header")).toObject();
        pl.headerEnabled = h.value(QLatin1String("enabled")).toBool(false);
        pl.headerLeft    = h.value(QLatin1String("left")).toString();
        pl.headerCenter  = h.value(QLatin1String("center")).toString();
        pl.headerRight   = h.value(QLatin1String("right")).toString();
    }
    if (plObj.contains(QLatin1String("footer"))) {
        QJsonObject f = plObj.value(QLatin1String("footer")).toObject();
        pl.footerEnabled = f.value(QLatin1String("enabled")).toBool(true);
        pl.footerLeft    = f.value(QLatin1String("left")).toString();
        pl.footerCenter  = f.value(QLatin1String("center")).toString();
        pl.footerRight   = f.value(QLatin1String("right")).toString(
            QStringLiteral("{page} / {pages}"));
    }
    // Note: pageBackground is NOT read here — palette owns that.

    // Master pages
    for (auto it = mpObj.begin(); it != mpObj.end(); ++it) {
        QJsonObject props = it.value().toObject();
        MasterPage mp;
        mp.name = it.key();

        if (props.contains(QLatin1String("headerEnabled")))
            mp.headerEnabled = props.value(QLatin1String("headerEnabled")).toBool() ? 1 : 0;
        if (props.contains(QLatin1String("footerEnabled")))
            mp.footerEnabled = props.value(QLatin1String("footerEnabled")).toBool() ? 1 : 0;

        if (props.contains(QLatin1String("headerLeft"))) {
            mp.headerLeft = props.value(QLatin1String("headerLeft")).toString();
            mp.hasHeaderLeft = true;
        }
        if (props.contains(QLatin1String("headerCenter"))) {
            mp.headerCenter = props.value(QLatin1String("headerCenter")).toString();
            mp.hasHeaderCenter = true;
        }
        if (props.contains(QLatin1String("headerRight"))) {
            mp.headerRight = props.value(QLatin1String("headerRight")).toString();
            mp.hasHeaderRight = true;
        }
        if (props.contains(QLatin1String("footerLeft"))) {
            mp.footerLeft = props.value(QLatin1String("footerLeft")).toString();
            mp.hasFooterLeft = true;
        }
        if (props.contains(QLatin1String("footerCenter"))) {
            mp.footerCenter = props.value(QLatin1String("footerCenter")).toString();
            mp.hasFooterCenter = true;
        }
        if (props.contains(QLatin1String("footerRight"))) {
            mp.footerRight = props.value(QLatin1String("footerRight")).toString();
            mp.hasFooterRight = true;
        }

        if (props.contains(QLatin1String("margins"))) {
            QJsonObject m = props.value(QLatin1String("margins")).toObject();
            if (m.contains(QLatin1String("top")))    mp.marginTop    = m.value(QLatin1String("top")).toDouble();
            if (m.contains(QLatin1String("bottom"))) mp.marginBottom = m.value(QLatin1String("bottom")).toDouble();
            if (m.contains(QLatin1String("left")))   mp.marginLeft   = m.value(QLatin1String("left")).toDouble();
            if (m.contains(QLatin1String("right")))  mp.marginRight  = m.value(QLatin1String("right")).toDouble();
        }

        pl.masterPages.insert(mp.name, mp);
    }

    return pl;
}

static QString pageSizeToString(QPageSize::PageSizeId id)
{
    switch (id) {
    case QPageSize::Letter: return QStringLiteral("Letter");
    case QPageSize::A5:     return QStringLiteral("A5");
    case QPageSize::Legal:  return QStringLiteral("Legal");
    case QPageSize::B5:     return QStringLiteral("B5");
    default:                return QStringLiteral("A4");
    }
}

QJsonObject PageLayout::toPageLayoutJson() const
{
    QJsonObject obj;

    obj[QLatin1String("pageSize")] = pageSizeToString(pageSizeId);
    obj[QLatin1String("orientation")] = (orientation == QPageLayout::Landscape)
        ? QStringLiteral("landscape") : QStringLiteral("portrait");

    QJsonObject m;
    m[QLatin1String("left")]   = margins.left();
    m[QLatin1String("top")]    = margins.top();
    m[QLatin1String("right")]  = margins.right();
    m[QLatin1String("bottom")] = margins.bottom();
    obj[QLatin1String("margins")] = m;

    QJsonObject h;
    h[QLatin1String("enabled")] = headerEnabled;
    h[QLatin1String("left")]    = headerLeft;
    h[QLatin1String("center")]  = headerCenter;
    h[QLatin1String("right")]   = headerRight;
    obj[QLatin1String("header")] = h;

    QJsonObject f;
    f[QLatin1String("enabled")] = footerEnabled;
    f[QLatin1String("left")]    = footerLeft;
    f[QLatin1String("center")]  = footerCenter;
    f[QLatin1String("right")]   = footerRight;
    obj[QLatin1String("footer")] = f;

    return obj;
}

QJsonObject PageLayout::toMasterPagesJson() const
{
    QJsonObject result;

    for (auto it = masterPages.begin(); it != masterPages.end(); ++it) {
        const MasterPage &mp = it.value();
        if (mp.isDefault())
            continue;

        QJsonObject props;

        if (mp.headerEnabled >= 0)
            props[QLatin1String("headerEnabled")] = (mp.headerEnabled != 0);
        if (mp.footerEnabled >= 0)
            props[QLatin1String("footerEnabled")] = (mp.footerEnabled != 0);

        if (mp.hasHeaderLeft)   props[QLatin1String("headerLeft")]   = mp.headerLeft;
        if (mp.hasHeaderCenter) props[QLatin1String("headerCenter")] = mp.headerCenter;
        if (mp.hasHeaderRight)  props[QLatin1String("headerRight")]  = mp.headerRight;
        if (mp.hasFooterLeft)   props[QLatin1String("footerLeft")]   = mp.footerLeft;
        if (mp.hasFooterCenter) props[QLatin1String("footerCenter")] = mp.footerCenter;
        if (mp.hasFooterRight)  props[QLatin1String("footerRight")]  = mp.footerRight;

        if (mp.marginTop >= 0 || mp.marginBottom >= 0
            || mp.marginLeft >= 0 || mp.marginRight >= 0) {
            QJsonObject m;
            if (mp.marginTop >= 0)    m[QLatin1String("top")]    = mp.marginTop;
            if (mp.marginBottom >= 0) m[QLatin1String("bottom")] = mp.marginBottom;
            if (mp.marginLeft >= 0)   m[QLatin1String("left")]   = mp.marginLeft;
            if (mp.marginRight >= 0)  m[QLatin1String("right")]  = mp.marginRight;
            props[QLatin1String("margins")] = m;
        }

        result[it.key()] = props;
    }

    return result;
}
