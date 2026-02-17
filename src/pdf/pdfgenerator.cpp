/*
 * pdfgenerator.cpp — Box tree → PDF content streams + font embedding
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "pdfgenerator.h"
#include "fontmanager.h"
#include "sfnt.h"
#include "headerfooterrenderer.h"

#include <QColor>
#include <QDateTime>
#include <QDebug>

#include <zlib.h>
#include FT_OUTLINE_H

PdfGenerator::PdfGenerator(FontManager *fontManager)
    : m_fontManager(fontManager)
{
}

void PdfGenerator::setDocumentInfo(const QString &filename, const QString &title)
{
    m_filename = filename;
    m_title = title;
}

// --- PDF coordinate helpers ---

QByteArray PdfGenerator::pdfCoord(qreal v)
{
    return QByteArray::number(v, 'f', 2);
}

QByteArray PdfGenerator::colorOperator(const QColor &color, bool fill)
{
    if (!color.isValid())
        return {};
    qreal r = color.redF();
    qreal g = color.greenF();
    qreal b = color.blueF();
    QByteArray op = fill ? " rg\n" : " RG\n";
    return pdfCoord(r) + " " + pdfCoord(g) + " " + pdfCoord(b) + op;
}

// --- Font registration ---

QByteArray PdfGenerator::pdfFontName(FontFace *face)
{
    int idx = ensureFontRegistered(face);
    return "F" + QByteArray::number(idx);
}

int PdfGenerator::ensureFontRegistered(FontFace *face)
{
    if (m_fontIndex.contains(face))
        return m_fontIndex[face];

    int idx = m_embeddedFonts.size();
    EmbeddedFont ef;
    ef.pdfName = "F" + QByteArray::number(idx);
    ef.face = face;
    m_embeddedFonts.append(ef);
    m_fontIndex[face] = idx;
    return idx;
}

// --- Main generate ---

QByteArray PdfGenerator::generate(const Layout::LayoutResult &layout,
                                   const PageLayout &pageLayout,
                                   const QString &title)
{
    m_embeddedFonts.clear();
    m_fontIndex.clear();
    m_embeddedImages.clear();
    m_imageIndex.clear();
    m_pageAnnotations.clear();
    m_glyphForms.clear();
    m_nextGlyphFormIdx = 0;
    if (m_title.isEmpty())
        m_title = title;

    QByteArray output;
    Pdf::Writer writer;
    if (!writer.openBuffer(&output))
        return {};

    writer.writeHeader();
    m_writer = &writer;

    int totalPages = layout.pages.size();

    // Detect Hershey mode
    m_hersheyMode = false;
    for (const auto &page : layout.pages) {
        for (const auto &elem : page.elements) {
            std::visit([&](const auto &e) {
                using T = std::decay_t<decltype(e)>;
                if constexpr (std::is_same_v<T, Layout::BlockBox>) {
                    for (const auto &line : e.lines)
                        for (const auto &gbox : line.glyphs)
                            if (gbox.font && gbox.font->isHershey)
                                m_hersheyMode = true;
                } else if constexpr (std::is_same_v<T, Layout::TableBox>) {
                    for (const auto &row : e.rows)
                        for (const auto &cell : row.cells)
                            for (const auto &line : cell.lines)
                                for (const auto &gbox : line.glyphs)
                                    if (gbox.font && gbox.font->isHershey)
                                        m_hersheyMode = true;
                } else if constexpr (std::is_same_v<T, Layout::FootnoteSectionBox>) {
                    for (const auto &fn : e.footnotes)
                        for (const auto &line : fn.lines)
                            for (const auto &gbox : line.glyphs)
                                if (gbox.font && gbox.font->isHershey)
                                    m_hersheyMode = true;
                }
            }, elem);
            if (m_hersheyMode) break;
        }
        if (m_hersheyMode) break;
    }

    // First pass: register fonts by scanning all glyph boxes
    for (const auto &page : layout.pages) {
        for (const auto &elem : page.elements) {
            std::visit([&](const auto &e) {
                using T = std::decay_t<decltype(e)>;
                if constexpr (std::is_same_v<T, Layout::BlockBox>) {
                    for (const auto &line : e.lines)
                        for (const auto &gbox : line.glyphs)
                            if (gbox.font && !gbox.font->isHershey)
                                ensureFontRegistered(gbox.font);
                } else if constexpr (std::is_same_v<T, Layout::TableBox>) {
                    for (const auto &row : e.rows)
                        for (const auto &cell : row.cells)
                            for (const auto &line : cell.lines)
                                for (const auto &gbox : line.glyphs)
                                    if (gbox.font && !gbox.font->isHershey)
                                        ensureFontRegistered(gbox.font);
                } else if constexpr (std::is_same_v<T, Layout::FootnoteSectionBox>) {
                    for (const auto &fn : e.footnotes)
                        for (const auto &line : fn.lines)
                            for (const auto &gbox : line.glyphs)
                                if (gbox.font && !gbox.font->isHershey)
                                    ensureFontRegistered(gbox.font);
                }
            }, elem);
        }
    }

    // Also scan for images
    for (const auto &page : layout.pages) {
        for (const auto &elem : page.elements) {
            if (auto *bb = std::get_if<Layout::BlockBox>(&elem)) {
                if (bb->type == Layout::BlockBox::ImageBlock && !bb->image.isNull())
                    ensureImageRegistered(bb->imageId, bb->image);
            }
        }
    }

    // Also register header/footer font
    FontFace *hfFont = m_fontManager->loadFont(QStringLiteral("Noto Sans"), 400, false);
    if (hfFont)
        ensureFontRegistered(hfFont);

    // Embed fonts
    embedFonts(writer);

    // Embed images
    embedImages(writer);

    // Build resource dictionary
    Pdf::ResourceDict resources;
    for (auto &ef : m_embeddedFonts) {
        if (ef.fontObjId)
            resources.fonts[ef.pdfName] = ef.fontObjId;
    }
    for (auto &ei : m_embeddedImages)
        resources.xObjects[ei.pdfName] = ei.objId;

    // Base 14 Helvetica for markdown invisible text in Hershey mode
    if (m_hersheyMode && m_exportOptions.markdownCopy) {
        Pdf::ObjId hvInvObj = writer.startObj();
        writer.write("<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>");
        writer.endObj(hvInvObj);
        resources.fonts["HvInv"] = hvInvObj;
    }
    m_resources = &resources;

    // Initialize per-page annotation lists
    m_pageAnnotations.resize(layout.pages.size());

    // Write pages
    QList<Pdf::ObjId> pageObjIds;
    for (int pi = 0; pi < layout.pages.size(); ++pi) {
        m_currentPageIndex = pi;
        QByteArray contentStream = renderPage(layout.pages[pi], pageLayout, resources);

        // Content stream object
        Pdf::ObjId contentObj = writer.startObj();
        writer.write("<<\n");
        writer.endObjectWithStream(contentObj, contentStream);

        // Write link annotation objects for this page
        QList<Pdf::ObjId> annotObjIds;
        for (const auto &annot : m_pageAnnotations[pi]) {
            Pdf::ObjId annotObj = writer.startObj();
            writer.write("<<\n/Type /Annot\n/Subtype /Link\n");
            writer.write("/Rect ["
                         + pdfCoord(annot.rect.left()) + " "
                         + pdfCoord(annot.rect.bottom()) + " "
                         + pdfCoord(annot.rect.right()) + " "
                         + pdfCoord(annot.rect.top()) + "]\n");
            writer.write("/Border [0 0 0]\n"); // no visible border
            writer.write("/A <</Type /Action /S /URI /URI "
                         + Pdf::toLiteralString(annot.href.toUtf8()) + ">>\n");
            writer.write(">>");
            writer.endObj(annotObj);
            annotObjIds.append(annotObj);
        }

        // Page object
        Pdf::ObjId pageObj = writer.startObj();
        writer.write("<<\n");
        writer.write("/Type /Page\n");
        writer.write("/Parent " + Pdf::toObjRef(writer.pagesObj()) + "\n");
        writer.write("/MediaBox [0 0 "
                     + Pdf::toPdf(layout.pageSize.width()) + " "
                     + Pdf::toPdf(layout.pageSize.height()) + "]\n");
        writer.write("/Contents " + Pdf::toObjRef(contentObj) + "\n");
        writer.write("/Resources ");
        writer.writeResourceDict(resources);
        if (!annotObjIds.isEmpty()) {
            writer.write("/Annots [");
            for (auto id : annotObjIds)
                writer.write(Pdf::toObjRef(id) + " ");
            writer.write("]\n");
        }
        writer.write(">>");
        writer.endObj(pageObj);
        pageObjIds.append(pageObj);
    }

    // Pages object
    writer.startObj(writer.pagesObj());
    writer.write("<<\n/Type /Pages\n/Kids [");
    for (auto id : pageObjIds)
        writer.write(Pdf::toObjRef(id) + " ");
    writer.write("]\n/Count " + Pdf::toPdf(pageObjIds.size()) + "\n>>");
    writer.endObj(writer.pagesObj());

    // PDF Bookmarks / Outline tree
    Pdf::ObjId outlineObj = writeOutlines(writer, pageObjIds, layout, pageLayout);

    // Info object
    writer.startObj(writer.infoObj());
    writer.write("<<\n");
    writer.write("/Producer " + Pdf::toLiteralString(QStringLiteral("PrettyReader")) + "\n");
    QString infoTitle = m_exportOptions.title.isEmpty() ? m_title : m_exportOptions.title;
    if (!infoTitle.isEmpty())
        writer.write("/Title " + Pdf::toLiteralString(Pdf::toUTF16(infoTitle)) + "\n");
    if (!m_exportOptions.author.isEmpty())
        writer.write("/Author " + Pdf::toLiteralString(Pdf::toUTF16(m_exportOptions.author)) + "\n");
    if (!m_exportOptions.subject.isEmpty())
        writer.write("/Subject " + Pdf::toLiteralString(Pdf::toUTF16(m_exportOptions.subject)) + "\n");
    if (!m_exportOptions.keywords.isEmpty())
        writer.write("/Keywords " + Pdf::toLiteralString(Pdf::toUTF16(m_exportOptions.keywords)) + "\n");
    writer.write("/CreationDate " + Pdf::toDateString(QDateTime::currentDateTime()) + "\n");
    writer.write(">>");
    writer.endObj(writer.infoObj());

    // Catalog object
    writer.startObj(writer.catalogObj());
    writer.write("<<\n/Type /Catalog\n/Pages " + Pdf::toObjRef(writer.pagesObj()) + "\n");
    if (outlineObj && m_exportOptions.includeBookmarks)
        writer.write("/Outlines " + Pdf::toObjRef(outlineObj) + "\n");
    // PageMode
    switch (m_exportOptions.initialView) {
    case PdfExportOptions::ShowBookmarks:
        if (outlineObj && m_exportOptions.includeBookmarks)
            writer.write("/PageMode /UseOutlines\n");
        break;
    case PdfExportOptions::ShowThumbnails:
        writer.write("/PageMode /UseThumbs\n");
        break;
    case PdfExportOptions::ViewerDefault:
    default:
        break;
    }
    // PageLayout
    switch (m_exportOptions.pageLayout) {
    case PdfExportOptions::SinglePage:
        writer.write("/PageLayout /SinglePage\n");
        break;
    case PdfExportOptions::Continuous:
        writer.write("/PageLayout /OneColumn\n");
        break;
    case PdfExportOptions::FacingPages:
        writer.write("/PageLayout /TwoColumnLeft\n");
        break;
    case PdfExportOptions::FacingPagesFirstAlone:
        writer.write("/PageLayout /TwoColumnRight\n");
        break;
    }
    writer.write(">>");
    writer.endObj(writer.catalogObj());

    // XRef and trailer
    writer.writeXrefAndTrailer();
    writer.close();

    m_writer = nullptr;
    m_resources = nullptr;
    return output;
}

bool PdfGenerator::generateToFile(const Layout::LayoutResult &layout,
                                   const PageLayout &pageLayout,
                                   const QString &title,
                                   const QString &filePath)
{
    QByteArray data = generate(layout, pageLayout, title);
    if (data.isEmpty())
        return false;
    QFile f(filePath);
    if (!f.open(QIODevice::WriteOnly))
        return false;
    f.write(data);
    return true;
}

// --- Page rendering ---

QByteArray PdfGenerator::renderPage(const Layout::Page &page,
                                     const PageLayout &pageLayout,
                                     const Pdf::ResourceDict &resources)
{
    Q_UNUSED(resources);
    QByteArray stream;

    QSizeF pageSize = QPageSize(pageLayout.pageSizeId).sizePoints();
    qreal pageWidth = pageSize.width();
    qreal pageHeight = pageSize.height();

    // Calculate content area
    qreal marginLeft = pageLayout.margins.left() * 72.0 / 25.4;
    qreal marginRight = pageLayout.margins.right() * 72.0 / 25.4;
    qreal marginTop = pageLayout.margins.top() * 72.0 / 25.4;
    qreal marginBottom = pageLayout.margins.bottom() * 72.0 / 25.4;
    Q_UNUSED(marginRight);

    qreal contentTop = pageHeight - marginTop;
    if (pageLayout.headerEnabled)
        contentTop -= (PageLayout::kHeaderHeight + PageLayout::kSeparatorGap);

    qreal originX = marginLeft;
    qreal originY = contentTop;

    // Render header/footer
    renderHeaderFooter(stream, pageLayout, page.pageNumber,
                       0 /* filled later */, pageWidth, pageHeight);

    // Render page elements
    for (const auto &elem : page.elements) {
        std::visit([&](const auto &e) {
            using T = std::decay_t<decltype(e)>;
            if constexpr (std::is_same_v<T, Layout::BlockBox>) {
                renderBlockBox(e, stream, originX, originY, pageHeight);
            } else if constexpr (std::is_same_v<T, Layout::TableBox>) {
                renderTableBox(e, stream, originX, originY, pageHeight);
            } else if constexpr (std::is_same_v<T, Layout::FootnoteSectionBox>) {
                renderFootnoteSectionBox(e, stream, originX, originY, pageHeight);
            }
        }, elem);
    }

    return stream;
}

// --- Block rendering ---

void PdfGenerator::renderBlockBox(const Layout::BlockBox &box, QByteArray &stream,
                                   qreal originX, qreal originY, qreal pageHeight)
{
    Q_UNUSED(pageHeight);
    qreal blockY = originY - box.y; // PDF Y is bottom-up

    // Background
    if (box.background.isValid()) {
        stream += "q\n";
        stream += colorOperator(box.background, true);
        qreal bgX = originX + box.x - box.padding;
        qreal bgY = blockY - box.height - box.padding;
        qreal bgW = box.width + box.padding * 2;
        qreal bgH = box.height + box.padding * 2;
        stream += pdfCoord(bgX) + " " + pdfCoord(bgY) + " "
                + pdfCoord(bgW) + " " + pdfCoord(bgH) + " re f\n";

        // Border
        if (box.borderWidth > 0 && box.borderColor.isValid()) {
            stream += colorOperator(box.borderColor, false);
            stream += pdfCoord(box.borderWidth) + " w\n";
            stream += pdfCoord(bgX) + " " + pdfCoord(bgY) + " "
                    + pdfCoord(bgW) + " " + pdfCoord(bgH) + " re S\n";
        }
        stream += "Q\n";
    }

    // Image block
    if (box.type == Layout::BlockBox::ImageBlock) {
        renderImageBlock(box, stream, originX, originY);
        return;
    }

    // Horizontal rule
    if (box.type == Layout::BlockBox::HRuleBlock) {
        if (m_exportOptions.markdownCopy) {
            stream += "/Span <</ActualText <" + toUtf16BeHex(QStringLiteral("---\n\n")) + ">>> BDC\n";
            // Poppler needs a text-showing operator inside the span to
            // extract the ActualText.  Emit an invisible glyph at the
            // rule's vertical position using any already-registered font.
            if (!m_embeddedFonts.isEmpty()) {
                qreal ruleY = blockY - box.height / 2;
                stream += "BT\n3 Tr\n";
                stream += "/" + m_embeddedFonts.first().pdfName + " 1 Tf\n";
                stream += "1 0 0 1 " + pdfCoord(originX) + " "
                        + pdfCoord(ruleY) + " Tm\n";
                stream += "<0000> Tj\n";
                stream += "0 Tr\nET\n";
            }
            stream += "EMC\n";
        }
        stream += "q\n";
        stream += "0.8 0.8 0.8 RG\n";
        stream += "0.5 w\n";
        qreal ruleY = blockY - box.height / 2;
        stream += pdfCoord(originX) + " " + pdfCoord(ruleY) + " m "
                + pdfCoord(originX + box.width) + " " + pdfCoord(ruleY) + " l S\n";
        stream += "Q\n";
        return;
    }

    // Blockquote left border
    if (box.hasBlockQuoteBorder && box.blockQuoteLevel > 0) {
        stream += "q\n";
        stream += "0.80 0.80 0.80 RG\n"; // light gray border
        stream += "2.0 w\n";
        // Draw a vertical line at the left edge of the blockquote indent
        qreal borderX = originX + box.blockQuoteIndent - 8.0;
        qreal borderTop = blockY + box.spaceBefore;
        qreal borderBottom = blockY - box.height - box.spaceAfter;
        stream += pdfCoord(borderX) + " " + pdfCoord(borderTop) + " m "
                + pdfCoord(borderX) + " " + pdfCoord(borderBottom) + " l S\n";
        stream += "Q\n";
    }

    // Markdown copy: code block opening fence
    // Always use backtick fencing — 4-space indented code blocks also get
    // fenced because Poppler can't map ActualText leading spaces to selectable
    // regions (no invisible text glyphs at those positions).
    bool isCodeBlock = (box.type == Layout::BlockBox::CodeBlockType);
    if (m_exportOptions.markdownCopy && isCodeBlock) {
        m_codeBlockLines = true;
        QString fence = QStringLiteral("```");
        if (!box.codeLanguage.isEmpty())
            fence += box.codeLanguage;
        fence += QLatin1Char('\n');
        stream += "/Span <</ActualText <" + toUtf16BeHex(fence) + ">>> BDC\n";
        // Invisible text anchor at block top
        if (!m_embeddedFonts.isEmpty()) {
            stream += "BT\n3 Tr\n";
            stream += "/" + m_embeddedFonts.first().pdfName + " 1 Tf\n";
            stream += "1 0 0 1 " + pdfCoord(originX) + " "
                    + pdfCoord(blockY) + " Tm\n";
            stream += "<0000> Tj\n";
            stream += "0 Tr\nET\n";
        }
        stream += "EMC\n";
    }

    // Lines
    qreal lineY = 0;
    for (int li = 0; li < box.lines.size(); ++li) {
        qreal lineX = originX + box.x;
        qreal lineAvailWidth = box.width;
        if (li == 0 && box.firstLineIndent != 0) {
            lineX += box.firstLineIndent;
            lineAvailWidth -= box.firstLineIndent;
        }
        renderLineBox(box.lines[li], stream, lineX, blockY - lineY,
                      pageHeight, lineAvailWidth);
        lineY += box.lines[li].height;
    }

    m_codeBlockLines = false;

    // Markdown copy: emit block separator.
    // List items use \n (tight spacing); paragraphs/headings use \n\n.
    // Code blocks get closing fence ``` with invisible text anchor
    // (Poppler ignores ActualText spans without text-showing operators).
    // Regular separators work without anchors (Poppler infers paragraph
    // breaks from the Y-position gap).
    if (m_exportOptions.markdownCopy) {
        QString sep;
        if (isCodeBlock)
            sep = QStringLiteral("```\n\n");
        else if (box.isListItem)
            sep = QStringLiteral("\n");
        else
            sep = QStringLiteral("\n\n");
        stream += "/Span <</ActualText <" + toUtf16BeHex(sep) + ">>> BDC\n";
        if (isCodeBlock && !m_embeddedFonts.isEmpty()) {
            qreal bottomY = blockY - box.height;
            stream += "BT\n3 Tr\n";
            stream += "/" + m_embeddedFonts.first().pdfName + " 1 Tf\n";
            stream += "1 0 0 1 " + pdfCoord(originX) + " "
                    + pdfCoord(bottomY) + " Tm\n";
            stream += "<0000> Tj\n";
            stream += "0 Tr\nET\n";
        }
        stream += "EMC\n";
    }
}

// --- Markdown copy: glyph outline path rendering ---

namespace {

QByteArray coord(qreal v) { return QByteArray::number(v, 'f', 2); }

struct OutlineCtx {
    QByteArray *stream;
    qreal scale;
    qreal tx, ty;
    FT_Vector last;
};

int outlineMoveTo(const FT_Vector *to, void *user) {
    auto *c = static_cast<OutlineCtx *>(user);
    *c->stream += coord(to->x * c->scale + c->tx) + " "
                + coord(to->y * c->scale + c->ty) + " m\n";
    c->last = *to;
    return 0;
}

int outlineLineTo(const FT_Vector *to, void *user) {
    auto *c = static_cast<OutlineCtx *>(user);
    *c->stream += coord(to->x * c->scale + c->tx) + " "
                + coord(to->y * c->scale + c->ty) + " l\n";
    c->last = *to;
    return 0;
}

int outlineConicTo(const FT_Vector *ctrl, const FT_Vector *to, void *user) {
    auto *c = static_cast<OutlineCtx *>(user);
    qreal cp1x = (c->last.x + 2.0 * ctrl->x) / 3.0;
    qreal cp1y = (c->last.y + 2.0 * ctrl->y) / 3.0;
    qreal cp2x = (to->x + 2.0 * ctrl->x) / 3.0;
    qreal cp2y = (to->y + 2.0 * ctrl->y) / 3.0;
    *c->stream += coord(cp1x * c->scale + c->tx) + " "
                + coord(cp1y * c->scale + c->ty) + " "
                + coord(cp2x * c->scale + c->tx) + " "
                + coord(cp2y * c->scale + c->ty) + " "
                + coord(to->x * c->scale + c->tx) + " "
                + coord(to->y * c->scale + c->ty) + " c\n";
    c->last = *to;
    return 0;
}

int outlineCubicTo(const FT_Vector *c1, const FT_Vector *c2, const FT_Vector *to, void *user) {
    auto *c = static_cast<OutlineCtx *>(user);
    *c->stream += coord(c1->x * c->scale + c->tx) + " "
                + coord(c1->y * c->scale + c->ty) + " "
                + coord(c2->x * c->scale + c->tx) + " "
                + coord(c2->y * c->scale + c->ty) + " "
                + coord(to->x * c->scale + c->tx) + " "
                + coord(to->y * c->scale + c->ty) + " c\n";
    c->last = *to;
    return 0;
}

} // anonymous namespace

void PdfGenerator::renderLineBox(const Layout::LineBox &line, QByteArray &stream,
                                  qreal originX, qreal originY, qreal pageHeight,
                                  qreal availWidth)
{
    Q_UNUSED(pageHeight);

    qreal x;
    qreal baselineY = originY - line.baseline;

    // Full justification: distribute extra space between real word gaps.
    // When the layout engine has populated JustifyInfo (wordGapCount > 0),
    // use the pre-computed per-gap and per-character spacing.  Otherwise
    // fall back to the legacy inline gap-counting logic.
    bool doJustify = false;
    qreal extraPerGap = 0;
    qreal extraPerChar = 0;

    if (line.alignment == Qt::AlignJustify && !line.isLastLine
        && line.glyphs.size() > 1 && line.width < availWidth) {
        if (line.justify.wordGapCount > 0) {
            // New path: use pre-computed JustifyInfo
            doJustify = true;
            extraPerGap = line.justify.extraWordSpacing;
            extraPerChar = line.justify.extraLetterSpacing;
            qDebug() << "[PDF] justify NEW path: gapCount=" << line.justify.wordGapCount
                     << "extraPerGap=" << extraPerGap << "extraPerChar=" << extraPerChar
                     << "lineWidth=" << line.width << "availWidth=" << availWidth
                     << "r=" << line.justify.adjustmentRatio;
        } else {
            // Legacy fallback: compute gaps inline (until layout engine populates JustifyInfo)
            int gapCount = 0;
            for (int i = 1; i < line.glyphs.size(); ++i) {
                if (line.glyphs[i].startsAfterSoftHyphen)
                    continue;
                if (line.glyphs[i].style.background.isValid()
                    && line.glyphs[i - 1].style.background.isValid()
                    && line.glyphs[i].style.background == line.glyphs[i - 1].style.background)
                    continue;
                if (line.glyphs[i - 1].isListMarker)
                    continue;
                gapCount++;
            }
            if (gapCount > 0) {
                qreal extraSpace = availWidth - line.width;
                qreal epg = extraSpace / gapCount;
                if (epg <= m_maxJustifyGap) {
                    doJustify = true;
                    extraPerGap = epg;
                }
            }
        }
    }

    if (!doJustify && line.alignment == Qt::AlignJustify && !line.isLastLine) {
        qDebug() << "[PDF] justify SKIPPED: glyphs=" << line.glyphs.size()
                 << "lineWidth=" << line.width << "availWidth=" << availWidth
                 << "justifyGapCount=" << line.justify.wordGapCount;
    }

    bool markdownMode = m_exportOptions.markdownCopy;

    // --- Phase 1: compute x-positions for each glyph box ---
    // Needed so we can emit the per-line ActualText span + invisible overlay
    // before the path rendering, all with correct positions.
    QList<qreal> glyphXPositions;
    if (markdownMode) {
        glyphXPositions.reserve(line.glyphs.size());
        if (doJustify) {
            qreal cx = originX;
            for (int i = 0; i < line.glyphs.size(); ++i) {
                glyphXPositions.append(cx);
                cx += line.glyphs[i].width;
                if (i < line.glyphs.size() - 1)
                    cx += extraPerChar * line.glyphs[i].glyphs.size();
                if (i < line.glyphs.size() - 1) {
                    bool skipGap = line.glyphs[i + 1].startsAfterSoftHyphen;
                    if (!skipGap && line.glyphs[i + 1].attachedToPrevious)
                        skipGap = true;
                    if (!skipGap && line.glyphs[i + 1].style.background.isValid()
                        && line.glyphs[i].style.background.isValid()
                        && line.glyphs[i + 1].style.background == line.glyphs[i].style.background)
                        skipGap = true;
                    if (!skipGap && line.glyphs[i].isListMarker)
                        skipGap = true;
                    if (!skipGap)
                        cx += extraPerGap;
                }
            }
        } else {
            qreal xOffset = 0;
            if (line.alignment == Qt::AlignCenter)
                xOffset = (availWidth - line.width) / 2;
            else if (line.alignment == Qt::AlignRight)
                xOffset = availWidth - line.width;
            qreal cx = originX + xOffset;
            for (int i = 0; i < line.glyphs.size(); ++i) {
                glyphXPositions.append(cx);
                cx += line.glyphs[i].width;
            }
        }
    }

    // --- Phase 2 (markdown): per-line ActualText + invisible text overlay ---
    // One BDC/EMC per visual line eliminates spurious mid-line breaks that
    // occurred with per-word spans (font changes within a line caused Poppler
    // to split text). All invisible text objects share the same baselineY.
    if (markdownMode && !line.glyphs.isEmpty()) {
        // Build line ActualText from glyph box markdown decorations.
        // The text shaper places inter-word spaces as the TRAILING character
        // of the preceding glyph box (e.g. "Everything " not " Everything").
        // Check each box's raw text for a trailing space to decide whether
        // to insert a space before the next word. This prevents false spaces
        // at hyphen/slash breaks ("code-" has no trailing space → no gap).
        QString lineText;
        for (int i = 0; i < line.glyphs.size(); ++i) {
            const auto &gbox = line.glyphs[i];
            QString word = gbox.text.trimmed();
            if (gbox.isListMarker) {
                // Convert visual bullet back to markdown list marker;
                // skip any markdown decorations inherited from content style
                word.replace(QChar(0x2022), QLatin1Char('-'));
                lineText += word;
            } else {
                lineText += gbox.mdPrefix + word + gbox.mdSuffix;
            }
            // Add space only if this glyph box's source text ends with
            // whitespace (indicating a real word boundary, not a hyphen
            // or slash split).
            if (i < line.glyphs.size() - 1) {
                bool trailingSpace = !gbox.text.isEmpty()
                                   && gbox.text.at(gbox.text.size() - 1).isSpace();
                if (trailingSpace)
                    lineText += QLatin1Char(' ');
            }
        }
        // Trailing separator:
        // - Code blocks: no trailing \n — Poppler infers line breaks from
        //   the invisible text Y-positions; adding \n creates blank lines.
        // - Paragraphs: space for non-last lines, \n for last.
        if (m_codeBlockLines) {
            // no trailing separator
        } else if (!line.isLastLine) {
            if (!line.glyphs.isEmpty() && line.glyphs.last().trailingSoftHyphen) {
                // Soft-hyphen break: word continues on next line, no separator
            } else {
                lineText += QLatin1Char(' ');
            }
        } else {
            lineText += QLatin1Char('\n');
        }

        stream += "/Span <</ActualText <" + toUtf16BeHex(lineText) + ">>> BDC\n";

        // Invisible text overlay: per-glyph Tm matching visual positions
        stream += "BT\n";
        stream += "3 Tr\n";
        for (int i = 0; i < line.glyphs.size(); ++i) {
            const auto &gbox = line.glyphs[i];
            if (gbox.glyphs.isEmpty() || !gbox.font)
                continue;
            QByteArray fontName;
            if (gbox.font && gbox.font->isHershey)
                fontName = "HvInv";
            else
                fontName = pdfFontName(gbox.font);
            stream += "/" + fontName + " " + pdfCoord(gbox.fontSize) + " Tf\n";
            qreal curX = glyphXPositions[i];
            for (const auto &g : gbox.glyphs) {
                qreal px = curX + g.xOffset;
                qreal py = baselineY - g.yOffset;
                if (gbox.style.superscript)
                    py += gbox.fontSize * 0.35;
                else if (gbox.style.subscript)
                    py -= gbox.fontSize * 0.15;
                stream += "1 0 0 1 " + pdfCoord(px) + " " + pdfCoord(py) + " Tm\n";
                stream += Pdf::toHexString16(static_cast<quint16>(g.glyphId)) + " Tj\n";
                curX += g.xAdvance;
            }
        }
        stream += "0 Tr\n";
        stream += "ET\n";
        stream += "EMC\n";

        // Path rendering for visible text
        for (int i = 0; i < line.glyphs.size(); ++i) {
            if (line.glyphs[i].font && line.glyphs[i].font->isHershey)
                renderHersheyGlyphBox(line.glyphs[i], stream, glyphXPositions[i], baselineY);
            else
                renderGlyphBoxAsPath(line.glyphs[i], stream, glyphXPositions[i], baselineY);
        }

        x = glyphXPositions.isEmpty() ? originX
            : glyphXPositions.last() + line.glyphs.last().width;
    }
    // --- Non-markdown: standard rendering ---
    else if (doJustify) {
        x = originX;
        for (int i = 0; i < line.glyphs.size(); ++i) {
            if (line.glyphs[i].font && line.glyphs[i].font->isHershey)
                renderHersheyGlyphBox(line.glyphs[i], stream, x, baselineY);
            else if (m_hersheyMode)
                renderGlyphBoxAsPath(line.glyphs[i], stream, x, baselineY);
            else
                renderGlyphBox(line.glyphs[i], stream, x, baselineY);
            x += line.glyphs[i].width;
            if (i < line.glyphs.size() - 1)
                x += extraPerChar * line.glyphs[i].glyphs.size();
            if (i < line.glyphs.size() - 1) {
                bool skipGap = line.glyphs[i + 1].startsAfterSoftHyphen;
                if (!skipGap && line.glyphs[i + 1].attachedToPrevious)
                    skipGap = true;
                if (!skipGap && line.glyphs[i + 1].style.background.isValid()
                    && line.glyphs[i].style.background.isValid()
                    && line.glyphs[i + 1].style.background == line.glyphs[i].style.background)
                    skipGap = true;
                if (!skipGap && line.glyphs[i].isListMarker)
                    skipGap = true;
                if (!skipGap)
                    x += extraPerGap;
            }
        }
    } else {
        qreal xOffset = 0;
        if (line.alignment == Qt::AlignCenter)
            xOffset = (availWidth - line.width) / 2;
        else if (line.alignment == Qt::AlignRight)
            xOffset = availWidth - line.width;

        x = originX + xOffset;
        for (const auto &gbox : line.glyphs) {
            if (gbox.font && gbox.font->isHershey)
                renderHersheyGlyphBox(gbox, stream, x, baselineY);
            else if (m_hersheyMode)
                renderGlyphBoxAsPath(gbox, stream, x, baselineY);
            else
                renderGlyphBox(gbox, stream, x, baselineY);
            x += gbox.width;
        }
    }

    // Render trailing hyphen for soft-hyphen line breaks
    if (line.showTrailingHyphen && !line.glyphs.isEmpty()) {
        const auto &lastGbox = line.glyphs.last();
        if (lastGbox.font) {
            if (lastGbox.font->isHershey && lastGbox.font->hersheyFont) {
                HersheyFont *hFont = lastGbox.font->hersheyFont;
                auto entry = ensureGlyphForm(hFont, uint(U'-'), lastGbox.font->hersheyBold);
                if (entry.objId != 0) {
                    qreal scale = lastGbox.fontSize / hFont->unitsPerEm();
                    stream += "q\n";
                    stream += colorOperator(lastGbox.style.foreground, false);
                    if (lastGbox.font->hersheyItalic) {
                        stream += pdfCoord(scale) + " 0 "
                                + pdfCoord(scale * 0.2126) + " " + pdfCoord(scale)
                                + " " + pdfCoord(x) + " " + pdfCoord(baselineY) + " cm\n";
                    } else {
                        stream += pdfCoord(scale) + " 0 0 " + pdfCoord(scale)
                                + " " + pdfCoord(x) + " " + pdfCoord(baselineY) + " cm\n";
                    }
                    stream += "/" + entry.pdfName + " Do\n";
                    stream += "Q\n";
                }
            } else if (lastGbox.font->ftFace) {
                FT_UInt hyphenGid = FT_Get_Char_Index(lastGbox.font->ftFace, '-');
                if (markdownMode) {
                    FT_Face face = lastGbox.font->ftFace;
                    if (FT_Load_Glyph(face, hyphenGid, FT_LOAD_NO_SCALE) == 0
                        && face->glyph->format == FT_GLYPH_FORMAT_OUTLINE) {
                        qreal scale = lastGbox.fontSize / face->units_per_EM;
                        FT_Outline_Funcs funcs = {};
                        funcs.move_to = outlineMoveTo;
                        funcs.line_to = outlineLineTo;
                        funcs.conic_to = outlineConicTo;
                        funcs.cubic_to = outlineCubicTo;
                        OutlineCtx ctx;
                        ctx.stream = &stream;
                        ctx.scale = scale;
                        ctx.tx = x;
                        ctx.ty = baselineY;
                        ctx.last = {0, 0};
                        stream += "q\n";
                        stream += colorOperator(lastGbox.style.foreground, true);
                        FT_Outline_Decompose(&face->glyph->outline, &funcs, &ctx);
                        stream += "f\nQ\n";
                    }
                } else {
                    m_fontManager->markGlyphUsed(lastGbox.font, hyphenGid);
                    QByteArray fontName = pdfFontName(lastGbox.font);
                    stream += "BT\n";
                    stream += "/" + fontName + " " + pdfCoord(lastGbox.fontSize) + " Tf\n";
                    stream += colorOperator(lastGbox.style.foreground, true);
                    stream += "1 0 0 1 " + pdfCoord(x) + " " + pdfCoord(baselineY) + " Tm\n";
                    stream += Pdf::toHexString16(static_cast<quint16>(hyphenGid)) + " Tj\n";
                    stream += "ET\n";
                }
            }
        }
    }
}

void PdfGenerator::renderCheckbox(const Layout::GlyphBox &gbox, QByteArray &stream,
                                   qreal x, qreal y)
{
    // Draw a checkbox as PDF vector paths, sized to the font
    qreal size = gbox.fontSize * 0.7;  // checkbox size relative to font
    qreal cx = x + 1.0;               // small left offset
    qreal cy = y - size * 0.03;       // baseline adjustment — vertically centered with text x-height
    qreal r = size * 0.12;            // corner radius
    qreal lw = size * 0.07;           // line width scales with size

    // Stroke color from text style, default to dark gray
    QColor strokeColor = gbox.style.foreground.isValid()
        ? gbox.style.foreground : QColor(0x33, 0x33, 0x33);

    stream += "q\n";
    stream += colorOperator(strokeColor, false); // stroke color
    stream += pdfCoord(lw) + " w\n";

    // Rounded rectangle path (clockwise from bottom-left)
    // Bottom edge
    stream += pdfCoord(cx + r) + " " + pdfCoord(cy) + " m\n";
    stream += pdfCoord(cx + size - r) + " " + pdfCoord(cy) + " l\n";
    // Bottom-right corner
    stream += pdfCoord(cx + size) + " " + pdfCoord(cy) + " "
            + pdfCoord(cx + size) + " " + pdfCoord(cy + r) + " v\n";
    // Right edge
    stream += pdfCoord(cx + size) + " " + pdfCoord(cy + size - r) + " l\n";
    // Top-right corner
    stream += pdfCoord(cx + size) + " " + pdfCoord(cy + size) + " "
            + pdfCoord(cx + size - r) + " " + pdfCoord(cy + size) + " v\n";
    // Top edge
    stream += pdfCoord(cx + r) + " " + pdfCoord(cy + size) + " l\n";
    // Top-left corner
    stream += pdfCoord(cx) + " " + pdfCoord(cy + size) + " "
            + pdfCoord(cx) + " " + pdfCoord(cy + size - r) + " v\n";
    // Left edge
    stream += pdfCoord(cx) + " " + pdfCoord(cy + r) + " l\n";
    // Bottom-left corner
    stream += pdfCoord(cx) + " " + pdfCoord(cy) + " "
            + pdfCoord(cx + r) + " " + pdfCoord(cy) + " v\n";

    if (gbox.checkboxState == Layout::GlyphBox::Checked) {
        // Fill with light accent, then stroke
        stream += "0.92 0.95 1.0 rg\n"; // light blue fill
        stream += "B\n"; // fill + stroke path

        // Checkmark path
        stream += colorOperator(strokeColor, false);
        stream += pdfCoord(lw * 1.5) + " w\n";
        stream += "1 J 1 j\n"; // round caps and joins
        qreal mx = cx + size * 0.20; // checkmark start
        qreal my = cy + size * 0.50;
        qreal kx = cx + size * 0.42; // checkmark knee
        qreal ky = cy + size * 0.25;
        qreal ex = cx + size * 0.82; // checkmark end
        qreal ey = cy + size * 0.78;
        stream += pdfCoord(mx) + " " + pdfCoord(my) + " m\n";
        stream += pdfCoord(kx) + " " + pdfCoord(ky) + " l\n";
        stream += pdfCoord(ex) + " " + pdfCoord(ey) + " l\n";
        stream += "S\n";
    } else {
        stream += "S\n"; // stroke only
    }

    stream += "Q\n";
}

void PdfGenerator::renderGlyphBox(const Layout::GlyphBox &gbox, QByteArray &stream,
                                   qreal x, qreal y)
{
    // Checkbox: render as vector graphic instead of font glyphs
    if (gbox.checkboxState != Layout::GlyphBox::NoCheckbox) {
        renderCheckbox(gbox, stream, x, y);
        return;
    }

    if (gbox.glyphs.isEmpty() || !gbox.font)
        return;

    // Inline code / character background — paint BEFORE text
    if (gbox.style.background.isValid()) {
        stream += "q\n";
        stream += colorOperator(gbox.style.background, true);
        stream += pdfCoord(x - 1) + " " + pdfCoord(y - gbox.descent - 1) + " "
                + pdfCoord(gbox.width + 2) + " " + pdfCoord(gbox.ascent + gbox.descent + 2)
                + " re f\n";
        stream += "Q\n";
    }

    QByteArray fontName = pdfFontName(gbox.font);

    stream += "BT\n";
    stream += "/" + fontName + " " + pdfCoord(gbox.fontSize) + " Tf\n";
    stream += colorOperator(gbox.style.foreground, true);

    qreal curX = x;
    for (const auto &g : gbox.glyphs) {
        // Position each glyph with Tm (text matrix)
        qreal gx = curX + g.xOffset;
        qreal gy = y - g.yOffset;

        // Superscript/subscript adjustment
        if (gbox.style.superscript)
            gy += gbox.fontSize * 0.35;
        else if (gbox.style.subscript)
            gy -= gbox.fontSize * 0.15;

        stream += "1 0 0 1 " + pdfCoord(gx) + " " + pdfCoord(gy) + " Tm\n";
        stream += Pdf::toHexString16(static_cast<quint16>(g.glyphId)) + " Tj\n";
        curX += g.xAdvance;
    }

    stream += "ET\n";

    // Underline
    if (gbox.style.underline) {
        stream += "q\n";
        stream += colorOperator(gbox.style.foreground, false);
        stream += "0.5 w\n";
        qreal uy = y - gbox.descent * 0.3;
        stream += pdfCoord(x) + " " + pdfCoord(uy) + " m "
                + pdfCoord(curX) + " " + pdfCoord(uy) + " l S\n";
        stream += "Q\n";
    }

    // Strikethrough
    if (gbox.style.strikethrough) {
        stream += "q\n";
        stream += colorOperator(gbox.style.foreground, false);
        stream += "0.5 w\n";
        qreal sy = y + gbox.ascent * 0.3;
        stream += pdfCoord(x) + " " + pdfCoord(sy) + " m "
                + pdfCoord(curX) + " " + pdfCoord(sy) + " l S\n";
        stream += "Q\n";
    }

    // Collect link annotation rect
    if (!gbox.style.linkHref.isEmpty()) {
        collectLinkRect(x, y, curX - x, gbox.ascent, gbox.descent,
                        gbox.style.linkHref);
    }
}

void PdfGenerator::renderGlyphBoxAsPath(const Layout::GlyphBox &gbox,
                                         QByteArray &stream,
                                         qreal x, qreal y)
{
    if (gbox.glyphs.isEmpty() || !gbox.font || !gbox.font->ftFace)
        return;

    FT_Face face = gbox.font->ftFace;
    qreal scale = gbox.fontSize / face->units_per_EM;

    // Background (inline code highlight)
    if (gbox.style.background.isValid()) {
        stream += "q\n";
        stream += colorOperator(gbox.style.background, true);
        stream += pdfCoord(x - 1) + " " + pdfCoord(y - gbox.descent - 1) + " "
                + pdfCoord(gbox.width + 2) + " " + pdfCoord(gbox.ascent + gbox.descent + 2)
                + " re f\n";
        stream += "Q\n";
    }

    stream += "q\n";
    stream += colorOperator(gbox.style.foreground, true);

    FT_Outline_Funcs funcs = {};
    funcs.move_to = outlineMoveTo;
    funcs.line_to = outlineLineTo;
    funcs.conic_to = outlineConicTo;
    funcs.cubic_to = outlineCubicTo;

    qreal curX = x;
    for (const auto &g : gbox.glyphs) {
        qreal gx = curX + g.xOffset;
        qreal gy = y - g.yOffset;

        if (gbox.style.superscript)
            gy += gbox.fontSize * 0.35;
        else if (gbox.style.subscript)
            gy -= gbox.fontSize * 0.15;

        if (FT_Load_Glyph(face, g.glyphId, FT_LOAD_NO_SCALE) == 0
            && face->glyph->format == FT_GLYPH_FORMAT_OUTLINE) {
            OutlineCtx ctx;
            ctx.stream = &stream;
            ctx.scale = scale;
            ctx.tx = gx;
            ctx.ty = gy;
            ctx.last = {0, 0};
            FT_Outline_Decompose(&face->glyph->outline, &funcs, &ctx);
        }
        curX += g.xAdvance;
    }

    stream += "f\n";
    stream += "Q\n";

    // Underline
    if (gbox.style.underline) {
        stream += "q\n";
        stream += colorOperator(gbox.style.foreground, false);
        stream += "0.5 w\n";
        qreal uy = y - gbox.descent * 0.3;
        stream += pdfCoord(x) + " " + pdfCoord(uy) + " m "
                + pdfCoord(curX) + " " + pdfCoord(uy) + " l S\n";
        stream += "Q\n";
    }

    // Strikethrough
    if (gbox.style.strikethrough) {
        stream += "q\n";
        stream += colorOperator(gbox.style.foreground, false);
        stream += "0.5 w\n";
        qreal sy = y + gbox.ascent * 0.3;
        stream += pdfCoord(x) + " " + pdfCoord(sy) + " m "
                + pdfCoord(curX) + " " + pdfCoord(sy) + " l S\n";
        stream += "Q\n";
    }

    // Collect link annotation rect
    if (!gbox.style.linkHref.isEmpty()) {
        collectLinkRect(x, y, curX - x, gbox.ascent, gbox.descent,
                        gbox.style.linkHref);
    }
}

void PdfGenerator::renderHersheyGlyphBox(const Layout::GlyphBox &gbox,
                                           QByteArray &stream,
                                           qreal x, qreal y)
{
    if (gbox.glyphs.isEmpty() || !gbox.font || !gbox.font->hersheyFont)
        return;

    HersheyFont *hFont = gbox.font->hersheyFont;
    qreal fontSize = gbox.fontSize;
    qreal scale = fontSize / hFont->unitsPerEm();

    // Background (inline code highlight) — paint before strokes
    if (gbox.style.background.isValid()) {
        stream += "q\n";
        stream += colorOperator(gbox.style.background, true);
        stream += pdfCoord(x - 1) + " " + pdfCoord(y - gbox.descent - 1) + " "
                + pdfCoord(gbox.width + 2) + " " + pdfCoord(gbox.ascent + gbox.descent + 2)
                + " re f\n";
        stream += "Q\n";
    }

    qreal curX = x;
    for (const auto &g : gbox.glyphs) {
        auto entry = ensureGlyphForm(hFont, g.glyphId, gbox.font->hersheyBold);
        if (entry.objId == 0) {
            curX += g.xAdvance;
            continue;
        }

        qreal gx = curX + g.xOffset;
        qreal gy = y - g.yOffset;

        // Superscript/subscript adjustment
        if (gbox.style.superscript)
            gy += fontSize * 0.35;
        else if (gbox.style.subscript)
            gy -= fontSize * 0.15;

        stream += "q\n";
        stream += colorOperator(gbox.style.foreground, false); // stroke color

        if (gbox.font->hersheyItalic) {
            // Scale + italic skew + translate
            // Matrix: [sx 0 sx*tan(12deg) sy tx ty]
            stream += pdfCoord(scale) + " 0 "
                    + pdfCoord(scale * 0.2126) + " " + pdfCoord(scale)
                    + " " + pdfCoord(gx) + " " + pdfCoord(gy) + " cm\n";
        } else {
            // Scale + translate only
            stream += pdfCoord(scale) + " 0 0 " + pdfCoord(scale)
                    + " " + pdfCoord(gx) + " " + pdfCoord(gy) + " cm\n";
        }

        stream += "/" + entry.pdfName + " Do\n";
        stream += "Q\n";

        curX += g.xAdvance;
    }

    // Underline
    if (gbox.style.underline) {
        stream += "q\n";
        stream += colorOperator(gbox.style.foreground, false);
        stream += "0.5 w\n";
        qreal uy = y - gbox.descent * 0.3;
        stream += pdfCoord(x) + " " + pdfCoord(uy) + " m "
                + pdfCoord(curX) + " " + pdfCoord(uy) + " l S\n";
        stream += "Q\n";
    }

    // Strikethrough
    if (gbox.style.strikethrough) {
        stream += "q\n";
        stream += colorOperator(gbox.style.foreground, false);
        stream += "0.5 w\n";
        qreal sy = y + gbox.ascent * 0.3;
        stream += pdfCoord(x) + " " + pdfCoord(sy) + " m "
                + pdfCoord(curX) + " " + pdfCoord(sy) + " l S\n";
        stream += "Q\n";
    }

    // Collect link annotation rect
    if (!gbox.style.linkHref.isEmpty()) {
        collectLinkRect(x, y, curX - x, gbox.ascent, gbox.descent,
                        gbox.style.linkHref);
    }
}

PdfGenerator::GlyphFormEntry PdfGenerator::ensureGlyphForm(
    const HersheyFont *font, uint glyphId, bool bold)
{
    // Preconditions: must be called during generate() with valid writer/resources
    if (!font || !m_writer || !m_resources)
        return {};

    GlyphFormKey key{font, glyphId, bold};
    auto it = m_glyphForms.find(key);
    if (it != m_glyphForms.end())
        return it.value();

    const HersheyGlyph *hGlyph = font->glyph(static_cast<char32_t>(glyphId));
    if (!hGlyph) {
        GlyphFormEntry dummy;
        return dummy;
    }

    // Build the Form stream in glyph-local coordinates.
    // Origin: left baseline (x=0 at leftBound, y=0 at baseline).
    // Coordinates are in Hershey font units (scaled at call site via cm).
    QByteArray formStream;
    formStream += "1 J 1 j\n"; // round cap & join

    // Stroke width in glyph units. Call site scales by fontSize/unitsPerEm,
    // so: strokeWidth_glyphUnits * (fontSize/upm) = 0.02 * fontSize
    // Therefore: strokeWidth_glyphUnits = 0.02 * upm
    qreal strokeWidth = 0.02 * font->unitsPerEm();
    if (bold)
        strokeWidth *= 1.8;
    formStream += pdfCoord(strokeWidth) + " w\n";

    for (const auto &stroke : hGlyph->strokes) {
        if (stroke.size() < 2)
            continue;
        qreal px = stroke[0].x() - hGlyph->leftBound;
        qreal py = stroke[0].y();
        formStream += pdfCoord(px) + " " + pdfCoord(py) + " m\n";
        for (int si = 1; si < stroke.size(); ++si) {
            px = stroke[si].x() - hGlyph->leftBound;
            py = stroke[si].y();
            formStream += pdfCoord(px) + " " + pdfCoord(py) + " l\n";
        }
        formStream += "S\n";
    }

    // BBox in glyph units
    qreal advW = hGlyph->rightBound - hGlyph->leftBound;
    qreal bboxBottom = -font->descent();
    qreal bboxTop = font->ascent();

    // Write the Form XObject to PDF
    Pdf::ObjId objId = m_writer->startObj();
    m_writer->write("<<\n/Type /XObject\n/Subtype /Form\n");
    m_writer->write("/BBox [0 " + pdfCoord(bboxBottom) + " "
                    + pdfCoord(advW) + " " + pdfCoord(bboxTop) + "]\n");
    m_writer->endObjectWithStream(objId, formStream);

    // Register in resource dict and cache
    GlyphFormEntry entry;
    entry.objId = objId;
    entry.pdfName = "HG" + QByteArray::number(m_nextGlyphFormIdx++);
    entry.advanceWidth = advW;
    m_glyphForms.insert(key, entry);

    m_resources->xObjects[entry.pdfName] = objId;

    return entry;
}

QByteArray PdfGenerator::toUtf16BeHex(const QString &text)
{
    QByteArray hex;
    hex += "FEFF";
    for (QChar ch : text)
        hex += QString::asprintf("%04X", ch.unicode()).toLatin1();
    return hex;
}

// --- Table rendering ---

void PdfGenerator::renderTableBox(const Layout::TableBox &box, QByteArray &stream,
                                   qreal originX, qreal originY, qreal pageHeight)
{
    qreal tableY = originY - box.y;
    qreal tableLeft = originX;
    qreal tableBottom = tableY - box.height;

    // === Pass 1: Cell backgrounds ===
    for (const auto &row : box.rows) {
        for (const auto &cell : row.cells) {
            if (cell.background.isValid()) {
                qreal cellX = tableLeft + cell.x;
                qreal cellY = tableY - cell.y;
                stream += "q\n";
                stream += colorOperator(cell.background, true);
                stream += pdfCoord(cellX) + " " + pdfCoord(cellY - cell.height) + " "
                        + pdfCoord(cell.width) + " " + pdfCoord(cell.height) + " re f\n";
                stream += "Q\n";
            }
        }
    }

    // === Pass 2: Cell content ===
    for (const auto &row : box.rows) {
        for (const auto &cell : row.cells) {
            qreal cellX = tableLeft + cell.x;
            qreal cellY = tableY - cell.y;
            qreal innerX = cellX + box.cellPadding;
            qreal innerY = cellY - box.cellPadding;
            qreal lineY = 0;
            for (const auto &line : cell.lines) {
                renderLineBox(line, stream, innerX, innerY - lineY,
                              pageHeight, cell.width - box.cellPadding * 2);
                lineY += line.height;
            }
        }
    }

    // === Pass 3: Grid borders ===
    stream += "q\n";

    // Inner horizontal lines (between rows)
    if (box.innerBorderWidth > 0 && box.innerBorderColor.isValid()) {
        stream += colorOperator(box.innerBorderColor, false);
        stream += pdfCoord(box.innerBorderWidth) + " w\n";
        qreal rowY = 0;
        for (int ri = 0; ri < box.rows.size() - 1; ++ri) {
            rowY += box.rows[ri].height;
            qreal lineY = tableY - rowY;
            // Skip header-bottom line (drawn separately with heavier weight)
            if (ri == box.headerRowCount - 1)
                continue;
            stream += pdfCoord(tableLeft) + " " + pdfCoord(lineY) + " m "
                    + pdfCoord(tableLeft + box.width) + " " + pdfCoord(lineY) + " l S\n";
        }
    }

    // Inner vertical lines (between columns)
    if (box.innerBorderWidth > 0 && box.innerBorderColor.isValid()
        && box.columnPositions.size() > 2) {
        stream += colorOperator(box.innerBorderColor, false);
        stream += pdfCoord(box.innerBorderWidth) + " w\n";
        for (int ci = 1; ci < box.columnPositions.size() - 1; ++ci) {
            qreal lineX = tableLeft + box.columnPositions[ci];
            stream += pdfCoord(lineX) + " " + pdfCoord(tableY) + " m "
                    + pdfCoord(lineX) + " " + pdfCoord(tableBottom) + " l S\n";
        }
    }

    // Header bottom border (heavier line under header row)
    if (box.headerRowCount > 0
        && box.headerBottomBorderWidth > 0
        && box.headerBottomBorderColor.isValid()) {
        qreal headerHeight = 0;
        for (int ri = 0; ri < box.headerRowCount && ri < box.rows.size(); ++ri)
            headerHeight += box.rows[ri].height;
        qreal hbY = tableY - headerHeight;
        stream += colorOperator(box.headerBottomBorderColor, false);
        stream += pdfCoord(box.headerBottomBorderWidth) + " w\n";
        stream += pdfCoord(tableLeft) + " " + pdfCoord(hbY) + " m "
                + pdfCoord(tableLeft + box.width) + " " + pdfCoord(hbY) + " l S\n";
    }

    // Outer border (on top of everything)
    if (box.borderWidth > 0 && box.borderColor.isValid()) {
        stream += colorOperator(box.borderColor, false);
        stream += pdfCoord(box.borderWidth) + " w\n";
        stream += pdfCoord(tableLeft) + " " + pdfCoord(tableBottom) + " "
                + pdfCoord(box.width) + " " + pdfCoord(box.height) + " re S\n";
    }

    stream += "Q\n";
}

// --- Footnote section rendering ---

void PdfGenerator::renderFootnoteSectionBox(const Layout::FootnoteSectionBox &box,
                                             QByteArray &stream,
                                             qreal originX, qreal originY,
                                             qreal pageHeight)
{
    qreal sectionY = originY - box.y;

    // Separator line
    if (box.showSeparator) {
        stream += "q\n";
        stream += "0.7 0.7 0.7 RG\n";
        stream += "0.5 w\n";
        qreal sepWidth = box.width * box.separatorLength;
        qreal sepY = sectionY;
        stream += pdfCoord(originX) + " " + pdfCoord(sepY) + " m "
                + pdfCoord(originX + sepWidth) + " " + pdfCoord(sepY) + " l S\n";
        stream += "Q\n";
    }

    for (const auto &fn : box.footnotes) {
        qreal fnY = sectionY - fn.y;

        // Render footnote label
        // (The label is included in the line content from the layout engine)

        qreal lineY = 0;
        for (const auto &line : fn.lines) {
            renderLineBox(line, stream, originX, fnY - lineY,
                          pageHeight, box.width);
            lineY += line.height;
        }
    }
}

// --- Header/Footer rendering ---

void PdfGenerator::renderHeaderFooter(QByteArray &stream, const PageLayout &pageLayout,
                                       int pageNumber, int totalPages,
                                       qreal pageWidth, qreal pageHeight)
{
    PageLayout resolved = pageLayout.resolvedForPage(pageNumber);

    auto resolveField = [&](const QString &field) -> QString {
        return HeaderFooterRenderer::resolveField(field, {pageNumber, totalPages,
                                                          m_filename, m_title});
    };

    auto drawFields = [&](const QString &left, const QString &center, const QString &right,
                          qreal rectY, qreal rectHeight) {
        FontFace *font = m_fontManager->loadFont(QStringLiteral("Noto Sans"), 400, false);
        if (!font || !font->ftFace) return;

        QByteArray fname = pdfFontName(font);
        qreal fontSize = 9.0;
        qreal marginLeft = pageLayout.margins.left() * 72.0 / 25.4;
        qreal marginRight = pageLayout.margins.right() * 72.0 / 25.4;
        qreal textY = rectY;

        stream += "BT\n";
        stream += "/" + fname + " " + pdfCoord(fontSize) + " Tf\n";
        stream += "0.53 0.53 0.53 rg\n"; // #888888

        // Helper: measure text width and collect glyph IDs
        auto measureText = [&](const QString &text) -> qreal {
            qreal w = 0;
            for (QChar ch : text) {
                FT_UInt gid = FT_Get_Char_Index(font->ftFace, ch.unicode());
                w += m_fontManager->glyphWidth(font, gid, fontSize);
            }
            return w;
        };

        // Helper: emit glyph string (marks glyphs used for subsetting)
        auto emitText = [&](const QString &text) {
            for (QChar ch : text) {
                FT_UInt gid = FT_Get_Char_Index(font->ftFace, ch.unicode());
                m_fontManager->markGlyphUsed(font, gid);
                stream += Pdf::toHexString16(static_cast<quint16>(gid)) + " Tj\n";
            }
        };

        // Left field
        QString leftText = resolveField(left);
        if (!leftText.isEmpty()) {
            stream += "1 0 0 1 " + pdfCoord(marginLeft) + " " + pdfCoord(textY) + " Tm\n";
            emitText(leftText);
        }

        // Center field
        QString centerText = resolveField(center);
        if (!centerText.isEmpty()) {
            qreal textWidth = measureText(centerText);
            qreal centerX = (pageWidth - textWidth) / 2;
            stream += "1 0 0 1 " + pdfCoord(centerX) + " " + pdfCoord(textY) + " Tm\n";
            emitText(centerText);
        }

        // Right field
        QString rightText = resolveField(right);
        if (!rightText.isEmpty()) {
            qreal textWidth = measureText(rightText);
            qreal rightX = pageWidth - marginRight - textWidth;
            stream += "1 0 0 1 " + pdfCoord(rightX) + " " + pdfCoord(textY) + " Tm\n";
            emitText(rightText);
        }

        stream += "ET\n";
    };

    qreal mTop = pageLayout.margins.top() * 72.0 / 25.4;
    qreal mBottom = pageLayout.margins.bottom() * 72.0 / 25.4;

    // Header
    if (resolved.headerEnabled) {
        qreal headerY = pageHeight - mTop + PageLayout::kSeparatorGap;
        drawFields(resolved.headerLeft, resolved.headerCenter, resolved.headerRight,
                   headerY, PageLayout::kHeaderHeight);

        // Separator line
        qreal sepY = pageHeight - mTop;
        qreal mLeft = pageLayout.margins.left() * 72.0 / 25.4;
        qreal mRight = pageLayout.margins.right() * 72.0 / 25.4;
        stream += "q\n0.53 0.53 0.53 RG\n0.5 w\n";
        stream += pdfCoord(mLeft) + " " + pdfCoord(sepY) + " m "
                + pdfCoord(pageWidth - mRight) + " " + pdfCoord(sepY) + " l S\n";
        stream += "Q\n";
    }

    // Footer
    if (resolved.footerEnabled) {
        qreal footerY = mBottom - PageLayout::kSeparatorGap;
        drawFields(resolved.footerLeft, resolved.footerCenter, resolved.footerRight,
                   footerY, PageLayout::kFooterHeight);

        // Separator line
        qreal sepY = mBottom + PageLayout::kFooterHeight;
        qreal mLeft = pageLayout.margins.left() * 72.0 / 25.4;
        qreal mRight = pageLayout.margins.right() * 72.0 / 25.4;
        stream += "q\n0.53 0.53 0.53 RG\n0.5 w\n";
        stream += pdfCoord(mLeft) + " " + pdfCoord(sepY) + " m "
                + pdfCoord(pageWidth - mRight) + " " + pdfCoord(sepY) + " l S\n";
        stream += "Q\n";
    }
}

// --- Image registration and embedding ---

int PdfGenerator::ensureImageRegistered(const QString &imageId, const QImage &image)
{
    if (m_imageIndex.contains(imageId))
        return m_imageIndex[imageId];

    int idx = m_embeddedImages.size();
    EmbeddedImage ei;
    ei.pdfName = "Im" + QByteArray::number(idx);
    ei.image = image;
    ei.width = image.width();
    ei.height = image.height();
    m_embeddedImages.append(ei);
    m_imageIndex[imageId] = idx;
    return idx;
}

void PdfGenerator::embedImages(Pdf::Writer &writer)
{
    for (auto &ei : m_embeddedImages) {
        // Convert image to raw RGB bytes
        QImage rgb = ei.image.convertToFormat(QImage::Format_RGB888);
        QByteArray rawData;
        rawData.reserve(rgb.width() * rgb.height() * 3);
        for (int y = 0; y < rgb.height(); ++y) {
            const uchar *line = rgb.constScanLine(y);
            rawData.append(reinterpret_cast<const char *>(line), rgb.width() * 3);
        }

        Pdf::ObjId imgObj = writer.startObj();
        writer.write("<<\n/Type /XObject\n/Subtype /Image\n");
        writer.write("/Width " + Pdf::toPdf(ei.width) + "\n");
        writer.write("/Height " + Pdf::toPdf(ei.height) + "\n");
        writer.write("/ColorSpace /DeviceRGB\n");
        writer.write("/BitsPerComponent 8\n");
        writer.endObjectWithStream(imgObj, rawData);
        ei.objId = imgObj;
    }
}

void PdfGenerator::renderImageBlock(const Layout::BlockBox &box, QByteArray &stream,
                                     qreal originX, qreal originY)
{
    if (box.image.isNull() || box.imageId.isEmpty())
        return;

    auto it = m_imageIndex.find(box.imageId);
    if (it == m_imageIndex.end())
        return;

    QByteArray imgName = m_embeddedImages[it.value()].pdfName;

    // PDF image rendering: translate + scale with cm, then paint with Do
    qreal imgX = originX + box.x;
    qreal imgY = originY - box.y - box.imageHeight;

    stream += "q\n";
    stream += pdfCoord(box.imageWidth) + " 0 0 "
            + pdfCoord(box.imageHeight) + " "
            + pdfCoord(imgX) + " " + pdfCoord(imgY) + " cm\n";
    stream += "/" + imgName + " Do\n";
    stream += "Q\n";
}

// --- Link annotations ---

void PdfGenerator::collectLinkRect(qreal x, qreal y, qreal width, qreal ascent,
                                    qreal descent, const QString &href)
{
    if (href.isEmpty() || m_currentPageIndex < 0
        || m_currentPageIndex >= m_pageAnnotations.size())
        return;

    // PDF coordinates: y is bottom-up, baseline at y, ascent goes up, descent goes down
    LinkAnnotation annot;
    annot.rect = QRectF(x, y - descent, width, ascent + descent);
    annot.href = href;
    m_pageAnnotations[m_currentPageIndex].append(annot);
}

// --- PDF Outline / Bookmarks ---

Pdf::ObjId PdfGenerator::writeOutlines(Pdf::Writer &writer,
                                        const QList<Pdf::ObjId> &pageObjIds,
                                        const Layout::LayoutResult &layout,
                                        const PageLayout &pageLayout)
{
    if (!m_exportOptions.includeBookmarks)
        return 0;

    // 1. Collect headings from layout pages
    QList<OutlineEntry> entries;

    QSizeF pageSize = QPageSize(pageLayout.pageSizeId).sizePoints();
    qreal pageHeight = pageSize.height();
    qreal marginTop = pageLayout.margins.top() * 72.0 / 25.4;
    qreal contentTop = pageHeight - marginTop;
    if (pageLayout.headerEnabled)
        contentTop -= (PageLayout::kHeaderHeight + PageLayout::kSeparatorGap);

    for (int pi = 0; pi < layout.pages.size(); ++pi) {
        for (const auto &elem : layout.pages[pi].elements) {
            if (auto *bb = std::get_if<Layout::BlockBox>(&elem)) {
                if (bb->headingLevel > 0 && bb->headingLevel <= m_exportOptions.bookmarkMaxDepth
                    && !bb->headingText.isEmpty()) {
                    OutlineEntry entry;
                    entry.title = bb->headingText;
                    entry.level = bb->headingLevel;
                    entry.pageIndex = pi;
                    // PDF y: position at top of heading with some breathing room
                    entry.destY = contentTop - bb->y + bb->spaceBefore;
                    entries.append(entry);
                }
            }
        }
    }

    if (entries.isEmpty())
        return 0;

    // 2. Build tree structure using stack-based parent finding
    //    (same algorithm as TocWidget)
    // parents[level] = index of last entry at that heading level
    int parents[7] = {-1, -1, -1, -1, -1, -1, -1};
    // parentOf[i] = index of parent entry (-1 = top-level, parented to root)
    QList<int> parentOf(entries.size(), -1);

    for (int i = 0; i < entries.size(); ++i) {
        int level = entries[i].level;

        // Find nearest ancestor with lower level
        int parentIdx = -1;
        for (int l = level - 1; l >= 1; --l) {
            if (parents[l] >= 0) {
                parentIdx = parents[l];
                break;
            }
        }
        parentOf[i] = parentIdx;

        if (parentIdx >= 0) {
            entries[parentIdx].childIndices.append(i);
        }

        parents[level] = i;
        // Clear deeper levels
        for (int l = level + 1; l <= 6; ++l)
            parents[l] = -1;
    }

    // 3. Reserve object IDs: one for root + one per entry
    Pdf::ObjId rootObjId = writer.newObject();
    for (auto &entry : entries)
        entry.objId = writer.newObject();

    // 4. Collect top-level entries (parentOf == -1)
    QList<int> topLevel;
    for (int i = 0; i < entries.size(); ++i) {
        if (parentOf[i] < 0)
            topLevel.append(i);
    }

    // 5. Helper: count all descendants recursively
    std::function<int(int)> countDescendants = [&](int idx) -> int {
        int count = 0;
        for (int childIdx : entries[idx].childIndices) {
            count += 1 + countDescendants(childIdx);
        }
        return count;
    };

    // 6. Write each outline entry
    for (int i = 0; i < entries.size(); ++i) {
        const auto &entry = entries[i];
        Pdf::ObjId parentObj = (parentOf[i] >= 0) ? entries[parentOf[i]].objId : rootObjId;

        // Find prev/next siblings within same parent
        const QList<int> &siblings = (parentOf[i] >= 0)
            ? entries[parentOf[i]].childIndices
            : topLevel;

        int siblingPos = siblings.indexOf(i);
        int prevIdx = (siblingPos > 0) ? siblings[siblingPos - 1] : -1;
        int nextIdx = (siblingPos < siblings.size() - 1) ? siblings[siblingPos + 1] : -1;

        writer.startObj(entry.objId);
        writer.write("<<\n");
        writer.write("/Title " + Pdf::toLiteralString(Pdf::toUTF16(entry.title)) + "\n");
        writer.write("/Parent " + Pdf::toObjRef(parentObj) + "\n");

        // Destination: page + XYZ position
        if (entry.pageIndex >= 0 && entry.pageIndex < pageObjIds.size()) {
            writer.write("/Dest [" + Pdf::toObjRef(pageObjIds[entry.pageIndex])
                         + " /XYZ 0 " + pdfCoord(entry.destY) + " null]\n");
        }

        if (prevIdx >= 0)
            writer.write("/Prev " + Pdf::toObjRef(entries[prevIdx].objId) + "\n");
        if (nextIdx >= 0)
            writer.write("/Next " + Pdf::toObjRef(entries[nextIdx].objId) + "\n");

        if (!entry.childIndices.isEmpty()) {
            writer.write("/First " + Pdf::toObjRef(entries[entry.childIndices.first()].objId) + "\n");
            writer.write("/Last " + Pdf::toObjRef(entries[entry.childIndices.last()].objId) + "\n");
            writer.write("/Count " + Pdf::toPdf(countDescendants(i)) + "\n");
        }

        writer.write(">>");
        writer.endObj(entry.objId);
    }

    // 7. Write root outline object

    int totalCount = 0;
    for (int idx : topLevel)
        totalCount += 1 + countDescendants(idx);

    writer.startObj(rootObjId);
    writer.write("<<\n/Type /Outlines\n");
    writer.write("/First " + Pdf::toObjRef(entries[topLevel.first()].objId) + "\n");
    writer.write("/Last " + Pdf::toObjRef(entries[topLevel.last()].objId) + "\n");
    writer.write("/Count " + Pdf::toPdf(totalCount) + "\n");
    writer.write(">>");
    writer.endObj(rootObjId);

    return rootObjId;
}

// --- Font embedding ---

void PdfGenerator::embedFonts(Pdf::Writer &writer)
{
    for (auto &ef : m_embeddedFonts) {
        if (m_hersheyMode)
            continue;
        ef.fontObjId = writeCidFont(writer, ef.face, ef.pdfName);
    }
}

Pdf::ObjId PdfGenerator::writeCidFont(Pdf::Writer &writer, FontFace *face,
                                       const QByteArray &pdfName)
{
    // 1. Subset the font
    sfnt::SubsetResult subset = m_fontManager->subsetFont(face);
    QByteArray fontData = subset.success ? subset.fontData : face->rawData;

    // 2. Embed font stream
    Pdf::ObjId fontStreamObj = writer.startObj();
    writer.write("<<\n");
    writer.endObjectWithStream(fontStreamObj, fontData);

    // 3. Font descriptor
    QString psName = m_fontManager->postScriptName(face);
    QByteArray psNameBytes = psName.toLatin1();
    if (subset.success)
        psNameBytes = "AAAAAA+" + psNameBytes;

    Pdf::ObjId fontDescObj = writer.startObj();
    writer.write("<<\n/Type /FontDescriptor\n");
    writer.write("/FontName " + Pdf::toName(psNameBytes) + "\n");

    QList<int> bbox = m_fontManager->fontBBox(face);
    writer.write("/FontBBox [" + Pdf::toPdf(bbox[0]) + " " + Pdf::toPdf(bbox[1])
                 + " " + Pdf::toPdf(bbox[2]) + " " + Pdf::toPdf(bbox[3]) + "]\n");
    writer.write("/Flags " + Pdf::toPdf(m_fontManager->fontFlags(face)) + "\n");
    writer.write("/Ascent " + Pdf::toPdf(static_cast<int>(
        m_fontManager->ascent(face, 1000.0 / m_fontManager->unitsPerEm(face) * 1000))) + "\n");
    writer.write("/Descent " + Pdf::toPdf(static_cast<int>(
        -m_fontManager->descent(face, 1000.0 / m_fontManager->unitsPerEm(face) * 1000))) + "\n");
    writer.write("/CapHeight " + Pdf::toPdf(static_cast<int>(
        m_fontManager->capHeight(face, 1000.0 / m_fontManager->unitsPerEm(face) * 1000))) + "\n");
    writer.write("/ItalicAngle " + Pdf::toPdf(m_fontManager->italicAngle(face)) + "\n");
    writer.write("/StemV 80\n");
    writer.write("/FontFile2 " + Pdf::toObjRef(fontStreamObj) + "\n");
    writer.write(">>");
    writer.endObj(fontDescObj);

    // 4. Glyph widths
    Pdf::ObjId widthsObj = writer.startObj();
    writer.write("[");
    qreal upem = m_fontManager->unitsPerEm(face);
    for (uint gid : face->usedGlyphs) {
        qreal w = m_fontManager->glyphWidth(face, gid, upem);
        int pdfWidth = static_cast<int>(w * 1000.0 / upem);
        writer.write(Pdf::toPdf(gid) + " [" + Pdf::toPdf(pdfWidth) + "] ");
    }
    writer.write("]");
    writer.endObj(widthsObj);

    // 5. ToUnicode CMap
    QByteArray cmapData = buildToUnicodeCMap(face);
    Pdf::ObjId cmapObj = writer.startObj();
    writer.write("<<\n");
    writer.endObjectWithStream(cmapObj, cmapData);

    // 6. Type0 font with CIDFont descendant
    Pdf::ObjId fontObj = writer.startObj();
    writer.write("<<\n/Type /Font\n/Subtype /Type0\n");
    writer.write("/Name " + Pdf::toName(pdfName) + "\n");
    writer.write("/BaseFont " + Pdf::toName(psNameBytes) + "\n");
    writer.write("/Encoding /Identity-H\n");
    writer.write("/ToUnicode " + Pdf::toObjRef(cmapObj) + "\n");
    writer.write("/DescendantFonts [");
    writer.write("<<\n/Type /Font\n/Subtype /CIDFontType2\n");
    writer.write("/BaseFont " + Pdf::toName(psNameBytes) + "\n");
    writer.write("/FontDescriptor " + Pdf::toObjRef(fontDescObj) + "\n");
    writer.write("/CIDSystemInfo <</Ordering(Identity)/Registry(Adobe)/Supplement 0>>\n");
    writer.write("/DW 1000\n");
    writer.write("/W " + Pdf::toObjRef(widthsObj) + "\n");
    writer.write("/CIDToGIDMap /Identity\n");
    writer.write(">>]\n");
    writer.write(">>");
    writer.endObj(fontObj);

    return fontObj;
}

QByteArray PdfGenerator::buildToUnicodeCMap(FontFace *face)
{
    QByteArray cmap;
    cmap += "/CIDInit /ProcSet findresource begin\n";
    cmap += "12 dict begin\n";
    cmap += "begincmap\n";
    cmap += "/CIDSystemInfo << /Registry (Adobe) /Ordering (UCS) /Supplement 0 >> def\n";
    cmap += "/CMapName /Adobe-Identity-UCS def\n";
    cmap += "/CMapType 2 def\n";
    cmap += "1 begincodespacerange\n<0000> <FFFF>\nendcodespacerange\n";

    // Build glyph-to-unicode mapping using FreeType's charmap
    QList<QPair<uint, uint>> mappings; // glyph ID → unicode
    if (face->ftFace) {
        FT_ULong charcode;
        FT_UInt gid;
        charcode = FT_Get_First_Char(face->ftFace, &gid);
        while (gid != 0) {
            if (face->usedGlyphs.contains(gid))
                mappings.append({gid, static_cast<uint>(charcode)});
            charcode = FT_Get_Next_Char(face->ftFace, charcode, &gid);
        }
    }

    // Write in batches of 100
    int pos = 0;
    while (pos < mappings.size()) {
        int batchSize = qMin(100, mappings.size() - pos);
        cmap += Pdf::toPdf(batchSize) + " beginbfchar\n";
        for (int i = 0; i < batchSize; ++i) {
            auto [gid, unicode] = mappings[pos + i];
            cmap += "<" + QString::asprintf("%04X", gid).toLatin1() + "> "
                  + "<" + QString::asprintf("%04X", unicode).toLatin1() + ">\n";
        }
        cmap += "endbfchar\n";
        pos += batchSize;
    }

    cmap += "endcmap\n";
    cmap += "CMapName currentdict /CMap defineresource pop\n";
    cmap += "end\nend\n";
    return cmap;
}
