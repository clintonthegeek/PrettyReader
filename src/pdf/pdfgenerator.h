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

#include "layoutengine.h"
#include "pagelayout.h"
#include "pdfwriter.h"

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
    void renderCheckbox(const Layout::GlyphBox &gbox, QByteArray &stream,
                        qreal x, qreal y);
    void renderLineBox(const Layout::LineBox &line, QByteArray &stream,
                       qreal originX, qreal originY, qreal pageHeight,
                       qreal availWidth);

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

    // Utility
    static QByteArray colorOperator(const QColor &color, bool fill = true);
    static QByteArray pdfCoord(qreal v);

    FontManager *m_fontManager;
    QString m_filename;
    QString m_title;
};

#endif // PRETTYREADER_PDFGENERATOR_H
