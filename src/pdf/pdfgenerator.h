/*
 * pdfgenerator.h — Box tree → PDF content streams + font embedding
 *
 * Converts Layout::LayoutResult into a complete PDF document.
 * Uses CIDFont Type 2 + Identity-H encoding for text,
 * DCTDecode/FlateDecode for images.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef PRETTYREADER_PDFGENERATOR_H
#define PRETTYREADER_PDFGENERATOR_H

#include <QByteArray>
#include <QString>

#include "hersheyfont.h"
#include "layoutengine.h"
#include "pagelayout.h"
#include "pdfwriter.h"
#include "pdfexportoptions.h"

class FontManager;
struct FontFace;

class PdfGenerator {
public:
    explicit PdfGenerator(FontManager *fontManager);

    QByteArray generate(const Layout::LayoutResult &layout,
                        const PageLayout &pageLayout,
                        const QString &title);
    bool generateToFile(const Layout::LayoutResult &layout,
                        const PageLayout &pageLayout,
                        const QString &title,
                        const QString &filePath);

    void setDocumentInfo(const QString &filename, const QString &title);
    void setMaxJustifyGap(qreal gap) { m_maxJustifyGap = gap; }
    void setExportOptions(const PdfExportOptions &opts) { m_exportOptions = opts; }

private:
    // Page content rendering
    QByteArray renderPage(const Layout::Page &page,
                          const PageLayout &pageLayout,
                          const Pdf::ResourceDict &resources);
    void renderBlockBox(const Layout::BlockBox &box, QByteArray &stream,
                        qreal originX, qreal originY, qreal pageHeight);
    void renderTableBox(const Layout::TableBox &box, QByteArray &stream,
                        qreal originX, qreal originY, qreal pageHeight);
    void renderFootnoteSectionBox(const Layout::FootnoteSectionBox &box,
                                  QByteArray &stream,
                                  qreal originX, qreal originY, qreal pageHeight);
    void renderGlyphBox(const Layout::GlyphBox &gbox, QByteArray &stream,
                        qreal x, qreal y);
    void renderGlyphBoxAsPath(const Layout::GlyphBox &gbox, QByteArray &stream,
                              qreal x, qreal y);
    void renderHersheyGlyphBox(const Layout::GlyphBox &gbox, QByteArray &stream,
                               qreal x, qreal y);
    void renderCheckbox(const Layout::GlyphBox &gbox, QByteArray &stream,
                        qreal x, qreal y);
    void renderLineBox(const Layout::LineBox &line, QByteArray &stream,
                       qreal originX, qreal originY, qreal pageHeight,
                       qreal availWidth);

    // Markdown copy mode: per-word ActualText encoding
    static QByteArray toUtf16BeHex(const QString &text);

    // Link annotations: collected per-page during rendering
    struct LinkAnnotation {
        QRectF rect;      // PDF coordinates (bottom-up)
        QString href;
    };
    QList<QList<LinkAnnotation>> m_pageAnnotations; // indexed by page number
    int m_currentPageIndex = 0;

    void collectLinkRect(qreal x, qreal y, qreal width, qreal ascent,
                         qreal descent, const QString &href);

    // Font embedding
    struct EmbeddedFont {
        Pdf::ObjId fontObjId = 0;
        QByteArray pdfName; // e.g. "F0", "F1"
        FontFace *face = nullptr;
    };
    QList<EmbeddedFont> m_embeddedFonts;
    QHash<FontFace *, int> m_fontIndex; // face → index in m_embeddedFonts

    QByteArray pdfFontName(FontFace *face);
    int ensureFontRegistered(FontFace *face);
    void embedFonts(Pdf::Writer &writer);
    Pdf::ObjId writeCidFont(Pdf::Writer &writer, FontFace *face,
                            const QByteArray &pdfName);
    QByteArray buildToUnicodeCMap(FontFace *face);

    // Header/footer rendering
    void renderHeaderFooter(QByteArray &stream, const PageLayout &pageLayout,
                            int pageNumber, int totalPages,
                            qreal pageWidth, qreal pageHeight);

    // Image embedding
    struct EmbeddedImage {
        Pdf::ObjId objId = 0;
        QByteArray pdfName; // e.g. "Im0", "Im1"
        QImage image;
        int width = 0;
        int height = 0;
    };
    QList<EmbeddedImage> m_embeddedImages;
    QHash<QString, int> m_imageIndex; // imageId → index in m_embeddedImages
    int ensureImageRegistered(const QString &imageId, const QImage &image);
    void embedImages(Pdf::Writer &writer);
    void renderImageBlock(const Layout::BlockBox &box, QByteArray &stream,
                          qreal originX, qreal originY);

    // PDF Outline / Bookmarks
    struct OutlineEntry {
        QString title;
        int level = 0;        // heading level 1-6
        int pageIndex = 0;    // index into pageObjIds
        qreal destY = 0;      // PDF y-coordinate for /Dest
        Pdf::ObjId objId = 0;
        QList<int> childIndices; // indices into flat list
    };

    Pdf::ObjId writeOutlines(Pdf::Writer &writer,
                              const QList<Pdf::ObjId> &pageObjIds,
                              const Layout::LayoutResult &layout,
                              const PageLayout &pageLayout);

    // Utility
    static QByteArray colorOperator(const QColor &color, bool fill = true);
    static QByteArray pdfCoord(qreal v);

    FontManager *m_fontManager;
    QString m_filename;
    QString m_title;
    qreal m_maxJustifyGap = 14.0;
    PdfExportOptions m_exportOptions;
    bool m_codeBlockLines = false; // true while rendering code block lines
    bool m_hersheyMode = false;
};

#endif // PRETTYREADER_PDFGENERATOR_H
