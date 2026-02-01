#include "printcontroller.h"

#include <QAbstractTextDocumentLayout>
#include <QDate>
#include <QFileDialog>
#include <QRegularExpression>
#include <QLocale>
#include <QPainter>
#include <QPrintDialog>
#include <QPrinter>
#include <QTextBlock>
#include <QTextDocument>

static constexpr qreal kHeaderHeight = 30.0;   // points
static constexpr qreal kFooterHeight = 20.0;   // points
static constexpr qreal kSeparatorGap = 8.0;    // points between header/footer and body

PrintController::PrintController(QTextDocument *document, QObject *parent)
    : QObject(parent)
    , m_document(document)
{
}

void PrintController::setPageSize(QPageSize::PageSizeId sizeId)
{
    m_pageSizeId = sizeId;
}

void PrintController::setOrientation(QPageLayout::Orientation orientation)
{
    m_orientation = orientation;
}

void PrintController::setMargins(const QMarginsF &margins)
{
    m_margins = margins;
}

void PrintController::setHeaderLeft(const QString &field) { m_headerLeft = field; }
void PrintController::setHeaderCenter(const QString &field) { m_headerCenter = field; }
void PrintController::setHeaderRight(const QString &field) { m_headerRight = field; }
void PrintController::setFooterLeft(const QString &field) { m_footerLeft = field; }
void PrintController::setFooterCenter(const QString &field) { m_footerCenter = field; }
void PrintController::setFooterRight(const QString &field) { m_footerRight = field; }
void PrintController::setHeaderEnabled(bool enabled) { m_headerEnabled = enabled; }
void PrintController::setFooterEnabled(bool enabled) { m_footerEnabled = enabled; }

void PrintController::setFileName(const QString &name) { m_fileName = name; }
void PrintController::setDocumentTitle(const QString &title) { m_documentTitle = title; }

void PrintController::configurePrinter(QPrinter *printer)
{
    printer->setPageSize(QPageSize(m_pageSizeId));
    printer->setPageOrientation(m_orientation);
    printer->setPageMargins(m_margins, QPageLayout::Millimeter);
}

void PrintController::print(QWidget *parentWidget)
{
    QPrinter printer(QPrinter::HighResolution);
    configurePrinter(&printer);

    QPrintDialog dialog(&printer, parentWidget);
    dialog.setWindowTitle(tr("Print Document"));

    if (dialog.exec() == QDialog::Accepted) {
        renderDocument(&printer);
    }
}

void PrintController::exportPdf(const QString &filePath, QWidget *parentWidget)
{
    QString path = filePath;
    if (path.isEmpty()) {
        path = QFileDialog::getSaveFileName(
            parentWidget, tr("Export as PDF"),
            QString(), tr("PDF Files (*.pdf)"));
        if (path.isEmpty())
            return;
    }

    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(path);
    configurePrinter(&printer);

    printer.setCreator(QStringLiteral("PrettyReader"));
    if (!m_documentTitle.isEmpty())
        printer.setDocName(m_documentTitle);

    renderDocument(&printer);
}

void PrintController::renderDocument(QPrinter *printer)
{
    QPainter painter(printer);
    if (!painter.isActive())
        return;

    // Scale painter from points to device pixels so all layout is in points
    qreal dpiScale = printer->resolution() / 72.0;
    painter.scale(dpiScale, dpiScale);

    // Get printable area in points
    QRectF pageRect = printer->pageRect(QPrinter::Point);

    // Calculate header/footer areas in points
    qreal headerH = m_headerEnabled ? kHeaderHeight : 0;
    qreal footerH = m_footerEnabled ? kFooterHeight : 0;
    qreal gap = kSeparatorGap;

    QRectF headerRect(0, 0, pageRect.width(), headerH);

    QRectF bodyRect(0,
                    headerH + (m_headerEnabled ? gap : 0),
                    pageRect.width(),
                    pageRect.height() - headerH - footerH
                        - (m_headerEnabled ? gap : 0)
                        - (m_footerEnabled ? gap : 0));

    QRectF footerRect(0,
                      pageRect.height() - footerH,
                      pageRect.width(),
                      footerH);

    // Set document page size in points so it paginates correctly
    m_document->setPageSize(bodyRect.size());
    int totalPages = m_document->pageCount();

    for (int page = 0; page < totalPages; ++page) {
        renderPage(&painter, page, totalPages,
                   headerRect, bodyRect, footerRect);

        if (page < totalPages - 1)
            printer->newPage();
    }

    painter.end();
}

void PrintController::renderPage(QPainter *painter, int pageNumber,
                                  int totalPages,
                                  const QRectF &headerRect,
                                  const QRectF &bodyRect,
                                  const QRectF &footerRect)
{
    // Header
    if (m_headerEnabled && headerRect.height() > 0) {
        painter->save();
        painter->setClipRect(headerRect);

        QFont headerFont(QStringLiteral("Noto Sans"));
        headerFont.setPixelSize(9);   // 9 "pixels" in scaled coords = 9pt
        painter->setFont(headerFont);
        painter->setPen(QColor(0x88, 0x88, 0x88));

        QString left = resolveField(m_headerLeft, pageNumber, totalPages);
        QString center = resolveField(m_headerCenter, pageNumber, totalPages);
        QString right = resolveField(m_headerRight, pageNumber, totalPages);

        if (!left.isEmpty())
            painter->drawText(headerRect, Qt::AlignLeft | Qt::AlignVCenter, left);
        if (!center.isEmpty())
            painter->drawText(headerRect, Qt::AlignHCenter | Qt::AlignVCenter, center);
        if (!right.isEmpty())
            painter->drawText(headerRect, Qt::AlignRight | Qt::AlignVCenter, right);

        // Separator line
        painter->setPen(QPen(QColor(0xcc, 0xcc, 0xcc), 1.0));
        painter->drawLine(headerRect.bottomLeft(), headerRect.bottomRight());
        painter->restore();
    }

    // Body content -- use documentLayout()->draw() with explicit palette
    // to avoid dark-theme QPalette bleeding light text onto white pages
    painter->save();
    painter->translate(bodyRect.topLeft());
    painter->translate(0, -pageNumber * bodyRect.height());

    QAbstractTextDocumentLayout::PaintContext ctx;
    ctx.clip = QRectF(0, pageNumber * bodyRect.height(),
                      bodyRect.width(), bodyRect.height());
    QPalette pal;
    pal.setColor(QPalette::Text, QColor(0x1a, 0x1a, 0x1a));
    pal.setColor(QPalette::Base, Qt::white);
    ctx.palette = pal;
    m_document->documentLayout()->draw(painter, ctx);
    painter->restore();

    // Footer
    if (m_footerEnabled && footerRect.height() > 0) {
        painter->save();
        painter->setClipRect(footerRect);

        // Separator line
        painter->setPen(QPen(QColor(0xcc, 0xcc, 0xcc), 1.0));
        painter->drawLine(footerRect.topLeft(), footerRect.topRight());

        QFont footerFont(QStringLiteral("Noto Sans"));
        footerFont.setPixelSize(9);   // 9 "pixels" in scaled coords = 9pt
        painter->setFont(footerFont);
        painter->setPen(QColor(0x88, 0x88, 0x88));

        QString left = resolveField(m_footerLeft, pageNumber, totalPages);
        QString center = resolveField(m_footerCenter, pageNumber, totalPages);
        QString right = resolveField(m_footerRight, pageNumber, totalPages);

        if (!left.isEmpty())
            painter->drawText(footerRect, Qt::AlignLeft | Qt::AlignVCenter, left);
        if (!center.isEmpty())
            painter->drawText(footerRect, Qt::AlignHCenter | Qt::AlignVCenter, center);
        if (!right.isEmpty())
            painter->drawText(footerRect, Qt::AlignRight | Qt::AlignVCenter, right);

        painter->restore();
    }
}

QString PrintController::resolveField(const QString &field, int pageNumber,
                                       int totalPages) const
{
    if (field.isEmpty())
        return {};

    QString result = field;
    result.replace(QLatin1String("{page}"), QString::number(pageNumber + 1));
    result.replace(QLatin1String("{pages}"), QString::number(totalPages));
    result.replace(QLatin1String("{filename}"), m_fileName);
    result.replace(QLatin1String("{title}"), m_documentTitle);
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
