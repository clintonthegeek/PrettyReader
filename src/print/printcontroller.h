#ifndef PRETTYREADER_PRINTCONTROLLER_H
#define PRETTYREADER_PRINTCONTROLLER_H

#include <QMarginsF>
#include <QObject>
#include <QPageLayout>
#include <QPageSize>

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

    // Page layout
    void setPageSize(QPageSize::PageSizeId sizeId);
    void setOrientation(QPageLayout::Orientation orientation);
    void setMargins(const QMarginsF &margins);

    // Header/footer fields (use placeholders like {page}, {pages}, {title})
    void setHeaderLeft(const QString &field);
    void setHeaderCenter(const QString &field);
    void setHeaderRight(const QString &field);
    void setFooterLeft(const QString &field);
    void setFooterCenter(const QString &field);
    void setFooterRight(const QString &field);
    void setHeaderEnabled(bool enabled);
    void setFooterEnabled(bool enabled);

    // Document metadata
    void setFileName(const QString &name);
    void setDocumentTitle(const QString &title);

private:
    void configurePrinter(QPrinter *printer);
    void renderDocument(QPrinter *printer);
    void renderPage(QPainter *painter, int pageNumber, int totalPages,
                    const QRectF &headerRect, const QRectF &bodyRect,
                    const QRectF &footerRect);
    QString resolveField(const QString &field, int pageNumber,
                         int totalPages) const;

    QTextDocument *m_document;

    // Page layout
    QPageSize::PageSizeId m_pageSizeId = QPageSize::A4;
    QPageLayout::Orientation m_orientation = QPageLayout::Portrait;
    QMarginsF m_margins{25.0, 25.0, 25.0, 25.0}; // mm

    // Header fields
    bool m_headerEnabled = false;
    QString m_headerLeft;
    QString m_headerCenter;
    QString m_headerRight;

    // Footer fields
    bool m_footerEnabled = true;
    QString m_footerLeft;
    QString m_footerCenter;
    QString m_footerRight{QStringLiteral("{page} / {pages}")};

    // Metadata
    QString m_fileName;
    QString m_documentTitle;
};

#endif // PRETTYREADER_PRINTCONTROLLER_H
