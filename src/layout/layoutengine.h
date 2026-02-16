/*
 * layoutengine.h — Content → box tree, line/page breaking
 *
 * Converts Content::Document into a paged box tree suitable for PDF rendering.
 * Uses TextShaper for HarfBuzz glyph shaping and ICU BreakIterator for
 * line break opportunities.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef PRETTYREADER_LAYOUTENGINE_H
#define PRETTYREADER_LAYOUTENGINE_H

#include <QColor>
#include <QImage>
#include <QList>
#include <QRectF>
#include <QSizeF>
#include <QString>

#include "contentmodel.h"
#include "pagelayout.h"

class FontManager;
class TextShaper;
struct FontFace;
struct ShapedRun;

namespace Layout {

// --- Box tree ---

struct GlyphInfo {
    uint glyphId = 0;
    qreal xAdvance = 0;
    qreal yAdvance = 0;
    qreal xOffset = 0;
    qreal yOffset = 0;
    int cluster = 0; // character index in source text
};

struct GlyphBox {
    enum CheckboxState { NoCheckbox, Unchecked, Checked };

    QList<GlyphInfo> glyphs;
    FontFace *font = nullptr;
    qreal fontSize = 0;
    Content::TextStyle style;
    qreal width = 0;
    qreal ascent = 0;
    qreal descent = 0;
    // Text range for search/selection
    int textStart = 0;
    int textLength = 0;
    bool rtl = false;
    bool trailingSoftHyphen = false; // word ended at a soft hyphen break point
    bool startsAfterSoftHyphen = false; // continues a soft-hyphenated word
    bool isListMarker = false; // bullet/number prefix — excluded from justify expansion
    // Task list checkbox (rendered as vector graphic, not font glyph)
    CheckboxState checkboxState = NoCheckbox;
};

struct ImageBox {
    QImage image;
    qreal width = 0;
    qreal height = 0;
    QString altText;
};

struct RuleBox {
    qreal width = 0;
    qreal thickness = 0.5;
    QColor color = QColor(0xcc, 0xcc, 0xcc);
};

struct LineBox {
    QList<GlyphBox> glyphs;
    QList<ImageBox> images; // inline images in this line
    qreal x = 0;
    qreal y = 0;
    qreal width = 0;
    qreal height = 0;
    qreal baseline = 0; // distance from top of line to baseline
    Qt::Alignment alignment = Qt::AlignLeft;
    bool isLastLine = false; // last line of paragraph (don't justify)
    bool showTrailingHyphen = false; // render '-' at end (soft hyphen break)
};

struct BlockBox {
    enum Type { ParagraphBlock, HeadingBlock, CodeBlockType, HRuleBlock, FootnoteSectionBlock, ImageBlock };
    Type type = ParagraphBlock;

    QList<LineBox> lines;
    qreal x = 0;
    qreal y = 0;
    qreal width = 0;
    qreal height = 0;
    qreal firstLineIndent = 0;
    qreal spaceBefore = 0;
    qreal spaceAfter = 0;
    QColor background; // invalid = none

    // Code block specifics
    qreal padding = 0;
    QColor borderColor;
    qreal borderWidth = 0;
    QString codeLanguage;

    // Heading level (0 = not heading)
    int headingLevel = 0;
    bool keepWithNext = false; // headings: don't strand at page bottom
    QString headingText;       // heading text for PDF bookmarks

    // Image block data (when type == ImageBlock)
    QImage image;
    qreal imageWidth = 0;
    qreal imageHeight = 0;
    QString imageId;    // unique ID for PDF XObject reference

    // Blockquote visual indicator
    bool hasBlockQuoteBorder = false;
    int blockQuoteLevel = 0;
    qreal blockQuoteIndent = 0;

    // Source breadcrumb
    Content::SourceRange source;
};

struct TableCellBox {
    QList<LineBox> lines;
    qreal x = 0;
    qreal y = 0;
    qreal width = 0;
    qreal height = 0;
    QColor background;
    Qt::Alignment alignment = Qt::AlignLeft;
    bool isHeader = false;
};

struct TableRowBox {
    QList<TableCellBox> cells;
    qreal y = 0;
    qreal height = 0;
};

struct TableBox {
    QList<TableRowBox> rows;
    qreal x = 0;
    qreal y = 0;
    qreal width = 0;
    qreal height = 0;
    int headerRowCount = 0;
    // Outer border
    qreal borderWidth = 0.5;
    QColor borderColor;
    // Inner grid border
    qreal innerBorderWidth = 0.5;
    QColor innerBorderColor;
    // Header bottom border
    qreal headerBottomBorderWidth = 2.0;
    QColor headerBottomBorderColor;
    qreal cellPadding = 4.0;
    // Column positions (x offsets) for grid drawing
    QList<qreal> columnPositions;

    // Source breadcrumb
    Content::SourceRange source;
};

struct FootnoteBox {
    QString label;
    QList<LineBox> lines;
    Content::TextStyle numberStyle;
    qreal y = 0;
    qreal height = 0;
};

struct FootnoteSectionBox {
    QList<FootnoteBox> footnotes;
    bool showSeparator = true;
    qreal separatorLength = 0.33;
    qreal x = 0;
    qreal y = 0;
    qreal width = 0;
    qreal height = 0;
};

// A page element can be any of the above
using PageElement = std::variant<BlockBox, TableBox, FootnoteSectionBox>;

// Source map: maps page-local rects to markdown source line ranges
struct SourceMapEntry {
    int pageNumber = -1;
    QRectF rect;        // page-local coordinates (points)
    int startLine = -1; // 1-based line in processed markdown
    int endLine = -1;   // 1-based line in processed markdown
};

// Code block hit region: maps page-local rect to content doc source lines
struct CodeBlockRegion {
    int pageNumber = -1;
    QRectF rect;        // page-local coordinates (points), includes padding
    int startLine = -1; // 1-based, matches Content::CodeBlock::source
    int endLine = -1;
};

struct Page {
    int pageNumber = 0;
    QList<PageElement> elements;
    qreal contentHeight = 0;
};

struct LayoutResult {
    QList<Page> pages;
    QSizeF pageSize; // in points
    QList<SourceMapEntry> sourceMap;
    QList<CodeBlockRegion> codeBlockRegions;
};

// --- Layout Engine ---

class Engine {
public:
    Engine(FontManager *fontManager, TextShaper *textShaper);

    LayoutResult layout(const Content::Document &doc, const PageLayout &pageLayout);

    void setHyphenateJustifiedText(bool enabled) { m_hyphenateJustifiedText = enabled; }

private:
    // Block layout
    BlockBox layoutParagraph(const Content::Paragraph &para, qreal availWidth);
    BlockBox layoutHeading(const Content::Heading &heading, qreal availWidth);
    BlockBox layoutCodeBlock(const Content::CodeBlock &cb, qreal availWidth);
    BlockBox layoutHorizontalRule(const Content::HorizontalRule &hr, qreal availWidth);
    BlockBox layoutImage(const Content::InlineImage &img, qreal availWidth);
    TableBox layoutTable(const Content::Table &table, qreal availWidth);
    FootnoteSectionBox layoutFootnoteSection(const Content::FootnoteSection &fs, qreal availWidth);

    // Blockquote layout: flattens children with indentation + left border
    QList<PageElement> layoutBlockQuote(const Content::BlockQuote &bq, qreal availWidth);

    // List layout: flattens list items into block boxes with indentation
    QList<PageElement> layoutList(const Content::List &list, qreal availWidth, int depth = 0);

    // Line breaking
    QList<LineBox> breakIntoLines(const QList<Content::InlineNode> &inlines,
                                  const Content::TextStyle &baseStyle,
                                  const Content::ParagraphFormat &format,
                                  qreal availWidth);

    QList<LineBox> shapeAndBreak(const QString &text,
                                 const Content::TextStyle &style,
                                 qreal availWidth,
                                 Qt::Alignment alignment);

    // Page assignment
    void assignToPages(const QList<PageElement> &elements,
                       const PageLayout &pageLayout,
                       LayoutResult &result);

    // Table splitting across pages (repeats header rows)
    QList<TableBox> splitTable(const TableBox &table, qreal availHeight, qreal pageHeight);

    // Helpers
    qreal measureInlines(const QList<Content::InlineNode> &inlines,
                          const Content::TextStyle &baseStyle);

    FontManager *m_fontManager;
    TextShaper *m_textShaper;
    bool m_hyphenateJustifiedText = true;
};

} // namespace Layout

#endif // PRETTYREADER_LAYOUTENGINE_H
