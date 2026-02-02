#include "printcontroller.h"
#include "headerfooterrenderer.h"

#include <QAbstractTextDocumentLayout>
#include <QFileDialog>
#include <QPainter>
#include <QPrintDialog>
#include <QPrinter>
#include <QTextDocument>

PrintController::PrintController(QTextDocument *document, QObject *parent)
    : QObject(parent)
    , m_document(document)
{
}

void PrintController::setPageLayout(const PageLayout &layout)
{
    m_pageLayout = layout;
}

void PrintController::setFileName(const QString &name) { m_fileName = name; }
void PrintController::setDocumentTitle(const QString &title) { m_documentTitle = title; }

void PrintController::configurePrinter(QPrinter *printer)
{
    printer->setPageSize(QPageSize(m_pageLayout.pageSizeId));
    printer->setPageOrientation(m_pageLayout.orientation);
    printer->setPageMargins(m_pageLayout.margins, QPageLayout::Millimeter);
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

    // Conversion factor from points to device pixels
    qreal dpiScale = printer->resolution() / 72.0;

    // Get printable area in device pixels
    QRectF pageRect = printer->pageRect(QPrinter::DevicePixel);

    // Calculate header/footer areas in device pixels
    qreal headerH = m_pageLayout.headerTotalHeight() * dpiScale;
    qreal footerH = m_pageLayout.footerTotalHeight() * dpiScale;

    QRectF headerRect(0, 0, pageRect.width(),
                      m_pageLayout.headerEnabled ? PageLayout::kHeaderHeight * dpiScale : 0);

    QRectF bodyRect(0,
                    headerH,
                    pageRect.width(),
                    pageRect.height() - headerH - footerH);

    QRectF footerRect(0,
                      pageRect.height() - (m_pageLayout.footerEnabled ? PageLayout::kFooterHeight * dpiScale : 0),
                      pageRect.width(),
                      m_pageLayout.footerEnabled ? PageLayout::kFooterHeight * dpiScale : 0);

    // Set document page size in device pixels for printer pagination
    m_document->setPageSize(bodyRect.size());
    int totalPages = m_document->pageCount();

    for (int page = 0; page < totalPages; ++page) {
        renderPage(&painter, page, totalPages,
                   headerRect, bodyRect, footerRect, dpiScale);

        if (page < totalPages - 1)
            printer->newPage();
    }

    painter.end();
}

void PrintController::renderPage(QPainter *painter, int pageNumber,
                                  int totalPages,
                                  const QRectF &headerRect,
                                  const QRectF &bodyRect,
                                  const QRectF &footerRect,
                                  qreal dpiScale)
{
    // Build metadata
    PageMetadata meta;
    meta.pageNumber = pageNumber;
    meta.totalPages = totalPages;
    meta.fileName = m_fileName;
    meta.title = m_documentTitle;

    // Header
    HeaderFooterRenderer::drawHeader(painter, headerRect, m_pageLayout, meta,
                                     1.0 * dpiScale);

    // Body content
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
    HeaderFooterRenderer::drawFooter(painter, footerRect, m_pageLayout, meta,
                                     1.0 * dpiScale);
}
