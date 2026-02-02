#include "headerfooterrenderer.h"
#include "pagelayout.h"

#include <QDate>
#include <QFont>
#include <QLocale>
#include <QPainter>
#include <QPen>
#include <QRegularExpression>

namespace HeaderFooterRenderer {

static void drawFields(QPainter *painter, const QRectF &rect,
                       const QString &left, const QString &center,
                       const QString &right, const PageMetadata &meta)
{
    QFont font(QStringLiteral("Noto Sans"));
    font.setPointSizeF(9.0);
    painter->setFont(font);
    painter->setPen(QColor(0x88, 0x88, 0x88));

    QString resolvedLeft = resolveField(left, meta);
    QString resolvedCenter = resolveField(center, meta);
    QString resolvedRight = resolveField(right, meta);

    if (!resolvedLeft.isEmpty())
        painter->drawText(rect, Qt::AlignLeft | Qt::AlignVCenter, resolvedLeft);
    if (!resolvedCenter.isEmpty())
        painter->drawText(rect, Qt::AlignHCenter | Qt::AlignVCenter, resolvedCenter);
    if (!resolvedRight.isEmpty())
        painter->drawText(rect, Qt::AlignRight | Qt::AlignVCenter, resolvedRight);
}

void drawHeader(QPainter *painter, const QRectF &rect,
                const PageLayout &layout, const PageMetadata &meta,
                qreal separatorWidth)
{
    if (!layout.headerEnabled || rect.height() <= 0)
        return;

    painter->save();
    painter->setClipRect(rect);

    drawFields(painter, rect,
               layout.headerLeft, layout.headerCenter, layout.headerRight,
               meta);

    // Separator line at bottom of header
    painter->setPen(QPen(QColor(0xcc, 0xcc, 0xcc), separatorWidth));
    painter->drawLine(rect.bottomLeft(), rect.bottomRight());

    painter->restore();
}

void drawFooter(QPainter *painter, const QRectF &rect,
                const PageLayout &layout, const PageMetadata &meta,
                qreal separatorWidth)
{
    if (!layout.footerEnabled || rect.height() <= 0)
        return;

    painter->save();
    painter->setClipRect(rect);

    // Separator line at top of footer
    painter->setPen(QPen(QColor(0xcc, 0xcc, 0xcc), separatorWidth));
    painter->drawLine(rect.topLeft(), rect.topRight());

    drawFields(painter, rect,
               layout.footerLeft, layout.footerCenter, layout.footerRight,
               meta);

    painter->restore();
}

QString resolveField(const QString &text, const PageMetadata &meta)
{
    if (text.isEmpty())
        return {};

    QString result = text;
    result.replace(QLatin1String("{page}"), QString::number(meta.pageNumber + 1));
    result.replace(QLatin1String("{pages}"), QString::number(meta.totalPages));
    result.replace(QLatin1String("{filename}"), meta.fileName);
    result.replace(QLatin1String("{title}"), meta.title);
    result.replace(QLatin1String("{date}"),
                   QLocale().toString(QDate::currentDate(), QLocale::ShortFormat));

    // Custom date format: {date:yyyy-MM-dd}
    static const QRegularExpression dateRx(
        QStringLiteral(R"(\{date:([^}]+)\})"));
    QRegularExpressionMatch match = dateRx.match(result);
    if (match.hasMatch()) {
        QString fmt = match.captured(1);
        result.replace(match.captured(0),
                       QDate::currentDate().toString(fmt));
    }

    return result;
}

} // namespace HeaderFooterRenderer
