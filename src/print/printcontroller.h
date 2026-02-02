#ifndef PRETTYREADER_PRINTCONTROLLER_H
#define PRETTYREADER_PRINTCONTROLLER_H

#include <QObject>

#include "pagelayout.h"

class QTextDocument;
class QPrinter;
class QPainter;

class PrintController : public QObject
{
    Q_OBJECT

public:
    explicit PrintController(QTextDocument *document,
                             QObject *parent = nullptr);

    void print(QWidget *parentWidget);
    void exportPdf(const QString &filePath, QWidget *parentWidget);

    void setPageLayout(const PageLayout &layout);

    // Document metadata
    void setFileName(const QString &name);
    void setDocumentTitle(const QString &title);

private:
    void configurePrinter(QPrinter *printer);
    void renderDocument(QPrinter *printer);
    void renderPage(QPainter *painter, int pageNumber, int totalPages,
                    const QRectF &headerRect, const QRectF &bodyRect,
                    const QRectF &footerRect, qreal dpiScale);

    QTextDocument *m_document;
    PageLayout m_pageLayout;

    // Metadata
    QString m_fileName;
    QString m_documentTitle;
};

#endif // PRETTYREADER_PRINTCONTROLLER_H
