/*
 * layoutengine.cpp — Content → box tree, line/page breaking
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "layoutengine.h"
#include "fontmanager.h"
#include "textshaper.h"

#include <QDebug>
#include <QMarginsF>
#include <QtMath>

#include <KSyntaxHighlighting/AbstractHighlighter>
#include <KSyntaxHighlighting/Definition>
#include <KSyntaxHighlighting/Format>
#include <KSyntaxHighlighting/Repository>
#include <KSyntaxHighlighting/State>
#include <KSyntaxHighlighting/Theme>

#include <unicode/brkiter.h>
#include <unicode/unistr.h>

namespace Layout {

Engine::Engine(FontManager *fontManager, TextShaper *textShaper)
    : m_fontManager(fontManager)
    , m_textShaper(textShaper)
{
}

// --- Main entry point ---

LayoutResult Engine::layout(const Content::Document &doc, const PageLayout &pageLayout)
{
    LayoutResult result;
    result.pageSize = QSizeF(
        pageLayout.pageSizeId == QPageSize::Custom
            ? 595.0  // A4 width fallback
            : QPageSize(pageLayout.pageSizeId).sizePoints().width(),
        pageLayout.pageSizeId == QPageSize::Custom
            ? 842.0  // A4 height fallback
            : QPageSize(pageLayout.pageSizeId).sizePoints().height());

    QSizeF contentSize = pageLayout.contentSizePoints();
    qreal availWidth = contentSize.width();

    // Layout all blocks into page elements
    QList<PageElement> elements;

    for (const auto &block : doc.blocks) {
        std::visit([&](const auto &b) {
            using T = std::decay_t<decltype(b)>;
            if constexpr (std::is_same_v<T, Content::Paragraph>) {
                // Detect image-only paragraphs (single InlineImage inline)
                if (b.inlines.size() == 1
                    && std::holds_alternative<Content::InlineImage>(b.inlines.first())) {
                    const auto &img = std::get<Content::InlineImage>(b.inlines.first());
                    if (!img.resolvedImageData.isEmpty()) {
                        elements.append(layoutImage(img, availWidth));
                    }
                } else {
                    elements.append(layoutParagraph(b, availWidth));
                }
            } else if constexpr (std::is_same_v<T, Content::Heading>) {
                elements.append(layoutHeading(b, availWidth));
            } else if constexpr (std::is_same_v<T, Content::CodeBlock>) {
                elements.append(layoutCodeBlock(b, availWidth));
            } else if constexpr (std::is_same_v<T, Content::BlockQuote>) {
                auto bqElements = layoutBlockQuote(b, availWidth);
                elements.append(bqElements);
            } else if constexpr (std::is_same_v<T, Content::List>) {
                auto listElements = layoutList(b, availWidth);
                elements.append(listElements);
            } else if constexpr (std::is_same_v<T, Content::Table>) {
                elements.append(layoutTable(b, availWidth));
            } else if constexpr (std::is_same_v<T, Content::HorizontalRule>) {
                elements.append(layoutHorizontalRule(b, availWidth));
            } else if constexpr (std::is_same_v<T, Content::FootnoteSection>) {
                elements.append(layoutFootnoteSection(b, availWidth));
            }
        }, block);
    }

    // Assign elements to pages
    assignToPages(elements, pageLayout, result);

    // Build source map from placed elements (maps page rects to markdown source lines)
    QMarginsF margins = pageLayout.marginsPoints();
    qreal headerOffset = pageLayout.headerTotalHeight();
    for (const auto &page : result.pages) {
        for (const auto &element : page.elements) {
            std::visit([&](const auto &e) {
                using T = std::decay_t<decltype(e)>;
                if constexpr (std::is_same_v<T, BlockBox>) {
                    if (e.source.startLine > 0) {
                        SourceMapEntry entry;
                        entry.pageNumber = page.pageNumber;
                        entry.rect = QRectF(margins.left() + e.x,
                                            margins.top() + headerOffset + e.y,
                                            e.width, e.height);
                        entry.startLine = e.source.startLine;
                        entry.endLine = e.source.endLine;
                        result.sourceMap.append(entry);

                        // Code block hit regions (includes padding for generous hit area)
                        if (e.type == BlockBox::CodeBlockType) {
                            CodeBlockRegion region;
                            region.pageNumber = page.pageNumber;
                            region.rect = QRectF(
                                margins.left() + e.x - e.padding,
                                margins.top() + headerOffset + e.y - e.padding,
                                e.width + e.padding * 2,
                                e.height + e.padding * 2);
                            region.startLine = e.source.startLine;
                            region.endLine = e.source.endLine;
                            result.codeBlockRegions.append(region);
                        }
                    }
                } else if constexpr (std::is_same_v<T, TableBox>) {
                    if (e.source.startLine > 0) {
                        SourceMapEntry entry;
                        entry.pageNumber = page.pageNumber;
                        entry.rect = QRectF(margins.left() + e.x,
                                            margins.top() + headerOffset + e.y,
                                            e.width, e.height);
                        entry.startLine = e.source.startLine;
                        entry.endLine = e.source.endLine;
                        result.sourceMap.append(entry);
                    }
                }
            }, element);
        }
    }

    return result;
}

// --- Inline text collection and shaping ---

namespace {

// Collect all text and style runs from inline nodes
struct CollectedText {
    QString text;
    QList<StyleRun> styleRuns;
    QList<Content::TextStyle> textStyles; // rendering styles, parallel to styleRuns
    QSet<int> softHyphenPositions;        // cleaned-text positions at soft hyphens
};

// Strip U+00AD (soft hyphen) from text, recording their cleaned-text positions
QString stripSoftHyphens(const QString &text, int offset, QSet<int> &positions)
{
    QString clean;
    clean.reserve(text.size());
    for (int i = 0; i < text.size(); ++i) {
        if (text[i] == QChar(0x00AD))
            positions.insert(offset + clean.size());
        else
            clean.append(text[i]);
    }
    return clean;
}

CollectedText collectInlines(const QList<Content::InlineNode> &inlines,
                              const Content::TextStyle &baseStyle)
{
    CollectedText result;
    for (const auto &node : inlines) {
        std::visit([&](const auto &n) {
            using T = std::decay_t<decltype(n)>;
            if constexpr (std::is_same_v<T, Content::TextRun>) {
                int startPos = result.text.size();
                QString clean = stripSoftHyphens(n.text, startPos, result.softHyphenPositions);
                StyleRun sr;
                sr.start = startPos;
                sr.length = clean.size();
                sr.fontFamily = n.style.fontFamily;
                sr.fontWeight = n.style.fontWeight;
                sr.fontItalic = n.style.italic;
                sr.fontSize = n.style.fontSize;
                sr.fontFeatures = n.style.fontFeatures;
                result.styleRuns.append(sr);
                result.textStyles.append(n.style);
                result.text.append(clean);
            } else if constexpr (std::is_same_v<T, Content::InlineCode>) {
                StyleRun sr;
                sr.start = result.text.size();
                sr.length = n.text.size();
                sr.fontFamily = n.style.fontFamily;
                sr.fontWeight = n.style.fontWeight;
                sr.fontItalic = n.style.italic;
                sr.fontSize = n.style.fontSize;
                sr.fontFeatures = n.style.fontFeatures;
                result.styleRuns.append(sr);
                result.textStyles.append(n.style);
                result.text.append(n.text);
            } else if constexpr (std::is_same_v<T, Content::FootnoteRef>) {
                StyleRun sr;
                sr.start = result.text.size();
                sr.length = n.label.size();
                sr.fontFamily = n.style.fontFamily;
                sr.fontWeight = n.style.fontWeight;
                sr.fontItalic = n.style.italic;
                sr.fontSize = n.style.fontSize;
                sr.fontFeatures = n.style.fontFeatures;
                result.styleRuns.append(sr);
                result.textStyles.append(n.style);
                result.text.append(n.label);
            } else if constexpr (std::is_same_v<T, Content::SoftBreak>) {
                // Treat as space
                StyleRun sr;
                sr.start = result.text.size();
                sr.length = 1;
                sr.fontFamily = baseStyle.fontFamily;
                sr.fontWeight = baseStyle.fontWeight;
                sr.fontItalic = baseStyle.italic;
                sr.fontSize = baseStyle.fontSize;
                result.styleRuns.append(sr);
                result.textStyles.append(baseStyle);
                result.text.append(QChar(' '));
            } else if constexpr (std::is_same_v<T, Content::HardBreak>) {
                // Append newline — handled during line breaking
                StyleRun sr;
                sr.start = result.text.size();
                sr.length = 1;
                sr.fontFamily = baseStyle.fontFamily;
                sr.fontWeight = baseStyle.fontWeight;
                sr.fontItalic = baseStyle.italic;
                sr.fontSize = baseStyle.fontSize;
                result.styleRuns.append(sr);
                result.textStyles.append(baseStyle);
                result.text.append(QChar('\n'));
            } else if constexpr (std::is_same_v<T, Content::Link>) {
                int startPos = result.text.size();
                QString clean = stripSoftHyphens(n.text, startPos, result.softHyphenPositions);
                StyleRun sr;
                sr.start = startPos;
                sr.length = clean.size();
                sr.fontFamily = n.style.fontFamily;
                sr.fontWeight = n.style.fontWeight;
                sr.fontItalic = n.style.italic;
                sr.fontSize = n.style.fontSize;
                result.styleRuns.append(sr);
                result.textStyles.append(n.style);
                result.text.append(clean);
            } else if constexpr (std::is_same_v<T, Content::InlineImage>) {
                // Placeholder for inline images — represented as object replacement
                // Images are handled separately during rendering
            }
        }, node);
    }
    return result;
}

// Resolve the Content::TextStyle for a character position in collected text
Content::TextStyle resolveStyleAt(int charPos, const CollectedText &collected,
                                   const Content::TextStyle &baseStyle)
{
    for (int i = 0; i < collected.styleRuns.size(); ++i) {
        const auto &sr = collected.styleRuns[i];
        if (sr.start <= charPos && sr.start + sr.length > charPos)
            return collected.textStyles[i];
    }
    return baseStyle;
}

} // anonymous namespace

// --- Line breaking ---

QList<LineBox> Engine::breakIntoLines(const QList<Content::InlineNode> &inlines,
                                       const Content::TextStyle &baseStyle,
                                       const Content::ParagraphFormat &format,
                                       qreal availWidth)
{
    QList<LineBox> lines;
    auto collected = collectInlines(inlines, baseStyle);
    if (collected.text.isEmpty())
        return lines;

    // Shape all text
    QList<ShapedRun> shapedRuns = m_textShaper->shape(collected.text, collected.styleRuns);

    // Use ICU BreakIterator to find line break opportunities
    UErrorCode err = U_ZERO_ERROR;
    icu::UnicodeString ustr(reinterpret_cast<const UChar *>(collected.text.utf16()),
                            collected.text.length());
    std::unique_ptr<icu::BreakIterator> lineBreakIter(
        icu::BreakIterator::createLineInstance(icu::Locale::getDefault(), err));
    if (U_FAILURE(err))
        lineBreakIter.reset();

    QSet<int> breakPositions;
    if (lineBreakIter) {
        lineBreakIter->setText(ustr);
        for (int32_t pos = lineBreakIter->first();
             pos != icu::BreakIterator::DONE;
             pos = lineBreakIter->next()) {
            breakPositions.insert(pos);
        }
    }

    // Add soft hyphen positions as additional break opportunities.
    // When HyphenateJustifiedText is off, skip them for justified paragraphs
    // so that justify relies only on word boundaries (wider gaps, no mid-word breaks).
    bool useSoftHyphens = !collected.softHyphenPositions.isEmpty();
    if (format.alignment == Qt::AlignJustify && !m_hyphenateJustifiedText)
        useSoftHyphens = false;
    if (useSoftHyphens)
        breakPositions.unite(collected.softHyphenPositions);

    // Build word-level glyph boxes by splitting shaped runs at break points.
    // Each shaped run may span multiple words. Using HarfBuzz cluster info
    // (character index per glyph), we split at ICU break positions and newlines.
    struct WordBox {
        GlyphBox gbox;
        bool isNewline = false;
    };
    QList<WordBox> words;

    for (const auto &run : shapedRuns) {
        // Resolve rendering style for this run
        Content::TextStyle runStyle = resolveStyleAt(run.textStart, collected, baseStyle);

        // Template for new glyph boxes from this run
        auto newGlyphBox = [&]() -> GlyphBox {
            GlyphBox gb;
            gb.font = run.font;
            gb.fontSize = run.fontSize;
            gb.style = runStyle;
            gb.rtl = run.rtl;
            gb.textStart = run.textStart;
            gb.textLength = 0;
            if (run.font) {
                gb.ascent = m_fontManager->ascent(run.font, run.fontSize);
                gb.descent = m_fontManager->descent(run.font, run.fontSize);
            } else {
                gb.ascent = run.fontSize * 0.8;
                gb.descent = run.fontSize * 0.2;
            }
            return gb;
        };

        GlyphBox currentWord = newGlyphBox();

        for (int gi = 0; gi < run.glyphs.size(); ++gi) {
            const auto &sg = run.glyphs[gi];
            int charPos = sg.cluster;

            // Check for newline character
            if (charPos < collected.text.size() && collected.text[charPos] == '\n') {
                // Finalize current word
                if (!currentWord.glyphs.isEmpty()) {
                    words.append({currentWord, false});
                    currentWord = newGlyphBox();
                }
                // Insert newline marker
                words.append({GlyphBox{}, true});
                continue;
            }

            // Check if this glyph starts at a line break opportunity
            if (breakPositions.contains(charPos) && !currentWord.glyphs.isEmpty()) {
                bool isSoftHyphenBreak = collected.softHyphenPositions.contains(charPos);
                if (isSoftHyphenBreak)
                    currentWord.trailingSoftHyphen = true;
                // Finalize current word and start a new one
                words.append({currentWord, false});
                currentWord = newGlyphBox();
                currentWord.textStart = charPos;
                if (isSoftHyphenBreak)
                    currentWord.startsAfterSoftHyphen = true;
            }

            // Add glyph to current word
            GlyphInfo info;
            info.glyphId = sg.glyphId;
            info.xAdvance = sg.xAdvance;
            info.yAdvance = sg.yAdvance;
            info.xOffset = sg.xOffset;
            info.yOffset = sg.yOffset;
            info.cluster = sg.cluster;
            currentWord.glyphs.append(info);
            currentWord.width += sg.xAdvance;
            currentWord.textLength = charPos - currentWord.textStart + 1;
        }

        // Finalize last word of run
        if (!currentWord.glyphs.isEmpty())
            words.append({currentWord, false});
    }

    // Greedy line breaking on word boxes
    qreal firstLineIndent = format.firstLineIndent;
    qreal lineWidth = availWidth - firstLineIndent;
    qreal currentX = 0;
    LineBox currentLine;
    currentLine.alignment = format.alignment;
    bool isFirstLine = true;

    for (int i = 0; i < words.size(); ++i) {
        const auto &word = words[i];

        // Newline: force line break
        if (word.isNewline) {
            currentLine.isLastLine = true; // don't justify newline-terminated lines
            lines.append(currentLine);
            currentLine = LineBox{};
            currentLine.alignment = format.alignment;
            currentX = 0;
            if (isFirstLine) {
                isFirstLine = false;
                lineWidth = availWidth;
            }
            continue;
        }

        // Check for line overflow (only break if there's content on this line)
        if (currentX + word.gbox.width > lineWidth && !currentLine.glyphs.isEmpty()) {
            // Show trailing hyphen if the last word on this line ends at a soft hyphen
            if (!currentLine.glyphs.isEmpty() && currentLine.glyphs.last().trailingSoftHyphen)
                currentLine.showTrailingHyphen = true;
            lines.append(currentLine);
            currentLine = LineBox{};
            currentLine.alignment = format.alignment;
            currentX = 0;
            if (isFirstLine) {
                isFirstLine = false;
                lineWidth = availWidth;
            }
        }

        // Force character-level breaks for words wider than line width
        // (e.g. long identifiers in table cells with no break opportunities)
        if (word.gbox.width > lineWidth && currentLine.glyphs.isEmpty()) {
            GlyphBox part = word.gbox;
            part.glyphs.clear();
            part.width = 0;
            part.textLength = 0;
            for (const auto &glyph : word.gbox.glyphs) {
                if (part.width + glyph.xAdvance > lineWidth && !part.glyphs.isEmpty()) {
                    currentLine.glyphs.append(part);
                    lines.append(currentLine);
                    currentLine = LineBox{};
                    currentLine.alignment = format.alignment;
                    part.glyphs.clear();
                    part.width = 0;
                    part.textLength = 0;
                }
                part.glyphs.append(glyph);
                part.width += glyph.xAdvance;
                part.textLength++;
            }
            if (!part.glyphs.isEmpty()) {
                currentLine.glyphs.append(part);
                currentX = part.width;
            }
            continue;
        }

        currentLine.glyphs.append(word.gbox);
        currentX += word.gbox.width;
    }

    // Don't forget the last line
    if (!currentLine.glyphs.isEmpty())
        lines.append(currentLine);

    // Mark the last line (don't justify it)
    if (!lines.isEmpty())
        lines.last().isLastLine = true;

    // Trim trailing whitespace glyphs from non-last lines.
    // ICU break positions place spaces at the end of the preceding word.
    // For justify, this trailing space inflates line.width and prevents the
    // last visible character from reaching the right margin.
    for (int li = 0; li < lines.size(); ++li) {
        if (lines[li].isLastLine || lines[li].glyphs.isEmpty())
            continue;
        auto &lastBox = lines[li].glyphs.last();
        while (!lastBox.glyphs.isEmpty()) {
            int cluster = lastBox.glyphs.last().cluster;
            if (cluster < collected.text.size() && collected.text[cluster].isSpace()) {
                lastBox.width -= lastBox.glyphs.last().xAdvance;
                lastBox.textLength--;
                lastBox.glyphs.removeLast();
            } else {
                break;
            }
        }
        // Remove the glyph box entirely if trimming emptied it,
        // otherwise justify counts it as a gap recipient
        if (lastBox.glyphs.isEmpty())
            lines[li].glyphs.removeLast();
    }

    // Compute line metrics
    for (auto &line : lines) {
        qreal maxAscent = 0;
        qreal maxDescent = 0;
        qreal totalWidth = 0;
        for (const auto &g : line.glyphs) {
            maxAscent = qMax(maxAscent, g.ascent);
            maxDescent = qMax(maxDescent, g.descent);
            totalWidth += g.width;
        }
        line.width = totalWidth;
        // Empty lines (e.g., blank lines in code) get minimum height from base style
        if (maxAscent + maxDescent < 1.0) {
            maxAscent = baseStyle.fontSize * 0.8;
            maxDescent = baseStyle.fontSize * 0.2;
        }
        line.baseline = maxAscent;
        line.height = maxAscent + maxDescent;
    }

    // Apply line height multiplier
    qreal lineHeightMult = format.lineHeightPercent / 100.0;
    for (auto &line : lines) {
        line.height *= lineHeightMult;
    }

    return lines;
}

// --- Block layout ---

BlockBox Engine::layoutParagraph(const Content::Paragraph &para, qreal availWidth)
{
    BlockBox box;
    box.type = BlockBox::ParagraphBlock;
    box.spaceBefore = para.format.spaceBefore;
    box.spaceAfter = para.format.spaceAfter;

    // Resolve base style from format
    Content::TextStyle baseStyle;
    if (!para.inlines.isEmpty()) {
        // Use style from first inline
        std::visit([&](const auto &n) {
            using T = std::decay_t<decltype(n)>;
            if constexpr (std::is_same_v<T, Content::TextRun>
                          || std::is_same_v<T, Content::InlineCode>
                          || std::is_same_v<T, Content::FootnoteRef>) {
                baseStyle = n.style;
            }
        }, para.inlines.first());
    }

    qreal effectiveWidth = availWidth - para.format.leftMargin - para.format.rightMargin;
    box.lines = breakIntoLines(para.inlines, baseStyle, para.format, effectiveWidth);
    box.width = effectiveWidth;

    // Compute total height
    qreal h = 0;
    for (const auto &line : box.lines)
        h += line.height;
    box.height = h;
    box.x = para.format.leftMargin;
    box.firstLineIndent = para.format.firstLineIndent;
    box.source = para.source;

    return box;
}

BlockBox Engine::layoutHeading(const Content::Heading &heading, qreal availWidth)
{
    BlockBox box;
    box.type = BlockBox::HeadingBlock;
    box.headingLevel = heading.level;
    box.width = availWidth;
    box.spaceBefore = heading.format.spaceBefore;
    box.spaceAfter = heading.format.spaceAfter;

    Content::TextStyle baseStyle;
    if (!heading.inlines.isEmpty()) {
        std::visit([&](const auto &n) {
            using T = std::decay_t<decltype(n)>;
            if constexpr (std::is_same_v<T, Content::TextRun>) {
                baseStyle = n.style;
            }
        }, heading.inlines.first());
    }

    box.lines = breakIntoLines(heading.inlines, baseStyle, heading.format, availWidth);
    box.keepWithNext = true;

    // Extract heading text for PDF bookmarks
    for (const auto &node : heading.inlines) {
        std::visit([&](const auto &n) {
            using T = std::decay_t<decltype(n)>;
            if constexpr (std::is_same_v<T, Content::TextRun>)
                box.headingText += n.text;
            else if constexpr (std::is_same_v<T, Content::InlineCode>)
                box.headingText += n.text;
            else if constexpr (std::is_same_v<T, Content::Link>)
                box.headingText += n.text;
        }, node);
    }

    qreal h = 0;
    for (const auto &line : box.lines)
        h += line.height;
    box.height = h;
    box.source = heading.source;

    return box;
}

// --- Code syntax highlighting adapter ---

namespace {

// Lightweight AbstractHighlighter subclass that collects styled spans
class CodeSpanCollector : public KSyntaxHighlighting::AbstractHighlighter
{
public:
    struct Span {
        int start;
        int length;
        QColor foreground;
        QColor background;
        bool bold = false;
        bool italic = false;
    };

    CodeSpanCollector()
    {
        static KSyntaxHighlighting::Repository repo;
        m_repo = &repo;
        auto defaultTheme = repo.defaultTheme(KSyntaxHighlighting::Repository::LightTheme);
        setTheme(defaultTheme);
    }

    QList<Span> highlight(const QString &code, const QString &language)
    {
        m_spans.clear();
        m_lineOffset = 0;

        auto def = m_repo->definitionForName(language);
        if (!def.isValid())
            def = m_repo->definitionForFileName(QStringLiteral("file.") + language);
        if (!def.isValid())
            return {};

        setDefinition(def);

        KSyntaxHighlighting::State state;
        const auto lines = code.split(QLatin1Char('\n'));
        for (const auto &line : lines) {
            state = highlightLine(line, state);
            m_lineOffset += line.size() + 1; // +1 for the \n
        }

        return m_spans;
    }

protected:
    void applyFormat(int offset, int length,
                     const KSyntaxHighlighting::Format &format) override
    {
        if (length == 0)
            return;

        Span span;
        span.start = m_lineOffset + offset;
        span.length = length;
        if (format.hasTextColor(theme()))
            span.foreground = format.textColor(theme());
        if (format.hasBackgroundColor(theme()))
            span.background = format.backgroundColor(theme());
        span.bold = format.isBold(theme());
        span.italic = format.isItalic(theme());
        m_spans.append(span);
    }

private:
    KSyntaxHighlighting::Repository *m_repo = nullptr;
    QList<Span> m_spans;
    int m_lineOffset = 0;
};

} // anonymous namespace

BlockBox Engine::layoutCodeBlock(const Content::CodeBlock &cb, qreal availWidth)
{
    BlockBox box;
    box.type = BlockBox::CodeBlockType;
    box.width = availWidth;
    box.padding = cb.padding;
    box.background = cb.background;
    box.borderColor = QColor(0xe1, 0xe4, 0xe8);
    box.borderWidth = 0.5;
    box.codeLanguage = cb.language;
    box.spaceBefore = 6.0;
    box.spaceAfter = 10.0;

    QList<Content::InlineNode> inlines;

    // Try syntax highlighting if language is known
    if (!cb.language.isEmpty()) {
        CodeSpanCollector collector;
        auto spans = collector.highlight(cb.code, cb.language);

        if (!spans.isEmpty()) {
            // Sort spans by start position
            std::sort(spans.begin(), spans.end(),
                      [](const auto &a, const auto &b) { return a.start < b.start; });

            int lastEnd = 0;
            for (const auto &span : spans) {
                // Gap between spans → default style
                if (span.start > lastEnd) {
                    Content::TextRun run;
                    run.text = cb.code.mid(lastEnd, span.start - lastEnd);
                    run.style = cb.style;
                    inlines.append(run);
                }
                // Highlighted span
                Content::TextRun run;
                run.text = cb.code.mid(span.start, span.length);
                run.style = cb.style; // base monospace style
                if (span.foreground.isValid())
                    run.style.foreground = span.foreground;
                if (span.background.isValid())
                    run.style.background = span.background;
                if (span.bold)
                    run.style.fontWeight = 700;
                if (span.italic)
                    run.style.italic = true;
                inlines.append(run);
                lastEnd = span.start + span.length;
            }
            // Trailing unstyled text
            if (lastEnd < cb.code.size()) {
                Content::TextRun run;
                run.text = cb.code.mid(lastEnd);
                run.style = cb.style;
                inlines.append(run);
            }
        }
    }

    // Fallback: plain monospace text
    if (inlines.isEmpty()) {
        Content::TextRun run;
        run.text = cb.code;
        run.style = cb.style;
        inlines.append(run);
    }

    Content::ParagraphFormat fmt;
    fmt.lineHeightPercent = 130; // more spacing for code
    qreal innerWidth = availWidth - cb.padding * 2 - 24.0; // margins
    box.lines = breakIntoLines(inlines, cb.style, fmt, innerWidth);

    qreal h = cb.padding * 2;
    for (const auto &line : box.lines)
        h += line.height;
    box.height = h;
    box.x = 12.0; // match DocumentBuilder margins
    box.source = cb.source;

    return box;
}

BlockBox Engine::layoutHorizontalRule(const Content::HorizontalRule &hr, qreal availWidth)
{
    BlockBox box;
    box.type = BlockBox::HRuleBlock;
    box.width = availWidth;
    box.height = 1.0;
    box.spaceBefore = hr.topMargin;
    box.spaceAfter = hr.bottomMargin;
    box.source = hr.source;
    return box;
}

// --- Table layout ---

TableBox Engine::layoutTable(const Content::Table &table, qreal availWidth)
{
    TableBox tbox;
    tbox.width = availWidth;
    tbox.borderWidth = table.borderWidth;
    tbox.borderColor = table.borderColor;
    tbox.innerBorderWidth = table.innerBorderWidth;
    tbox.innerBorderColor = table.innerBorderColor;
    tbox.headerBottomBorderWidth = table.headerBottomBorderWidth;
    tbox.headerBottomBorderColor = table.headerBottomBorderColor;
    tbox.cellPadding = table.cellPadding;
    tbox.headerRowCount = table.headerRowCount;

    if (table.rows.isEmpty())
        return tbox;

    int numCols = 0;
    for (const auto &row : table.rows)
        numCols = qMax(numCols, row.cells.size());

    // Edge-to-edge column widths (no gap for borders)
    qreal colWidth = (numCols > 0) ? availWidth / numCols : availWidth;

    // Store column positions for grid drawing
    for (int i = 0; i <= numCols; ++i)
        tbox.columnPositions.append(colWidth * i);

    qreal y = 0;
    int rowIdx = 0;
    for (const auto &row : table.rows) {
        TableRowBox rbox;
        rbox.y = y;

        qreal maxCellHeight = 0;
        qreal x = 0;

        for (int ci = 0; ci < row.cells.size(); ++ci) {
            const auto &cell = row.cells[ci];
            TableCellBox cbox;
            cbox.x = x;
            cbox.y = y;
            cbox.width = colWidth;
            cbox.alignment = cell.alignment;
            cbox.isHeader = cell.isHeader;

            // Resolve cell background: explicit cell bg > alternating row > body bg
            if (cell.background.isValid()) {
                cbox.background = cell.background;
            } else if (!cell.isHeader) {
                int bodyRowIdx = rowIdx - table.headerRowCount;
                if (bodyRowIdx >= 0 && (bodyRowIdx % 2) == 1 && table.alternateRowColor.isValid())
                    cbox.background = table.alternateRowColor;
                else if (table.bodyBackground.isValid())
                    cbox.background = table.bodyBackground;
            }

            // Layout cell content
            Content::ParagraphFormat cellFmt;
            cellFmt.alignment = cell.alignment;
            Content::TextStyle cellStyle = cell.style;
            if (cellStyle.fontFamily.isEmpty()) {
                cellStyle.fontFamily = QStringLiteral("Noto Serif");
                cellStyle.fontSize = 11.0;
            }
            qreal innerWidth = colWidth - table.cellPadding * 2;
            cbox.lines = breakIntoLines(cell.inlines, cellStyle, cellFmt, innerWidth);

            qreal h = table.cellPadding * 2;
            for (const auto &line : cbox.lines)
                h += line.height;
            cbox.height = h;
            maxCellHeight = qMax(maxCellHeight, h);

            rbox.cells.append(cbox);
            x += colWidth;
        }

        // Equalize cell heights
        rbox.height = maxCellHeight;
        for (auto &c : rbox.cells)
            c.height = maxCellHeight;

        tbox.rows.append(rbox);
        y += maxCellHeight;
        rowIdx++;
    }

    tbox.height = y;
    tbox.source = table.source;
    return tbox;
}

// --- Image layout ---

BlockBox Engine::layoutImage(const Content::InlineImage &img, qreal availWidth)
{
    BlockBox box;
    box.type = BlockBox::ImageBlock;
    box.width = availWidth;
    box.spaceBefore = 6.0;
    box.spaceAfter = 6.0;

    QImage qimg;
    qimg.loadFromData(img.resolvedImageData);
    if (qimg.isNull()) {
        box.height = 0;
        return box;
    }

    // Scale image to fit available width, cap height at 500pt
    static constexpr qreal kMaxImageHeight = 500.0;
    qreal imgW = img.width > 0 ? img.width : qimg.width();
    qreal imgH = img.height > 0 ? img.height : qimg.height();

    if (imgW > availWidth) {
        qreal scale = availWidth / imgW;
        imgW = availWidth;
        imgH *= scale;
    }
    if (imgH > kMaxImageHeight) {
        qreal scale = kMaxImageHeight / imgH;
        imgH = kMaxImageHeight;
        imgW *= scale;
    }

    box.image = qimg.convertToFormat(QImage::Format_RGB888);
    box.imageWidth = imgW;
    box.imageHeight = imgH;
    box.height = imgH;
    // Generate unique image ID based on data hash
    box.imageId = QStringLiteral("Img%1").arg(
        QString::number(qHash(img.resolvedImageData), 16));

    return box;
}

// --- Blockquote layout ---

QList<PageElement> Engine::layoutBlockQuote(const Content::BlockQuote &bq, qreal availWidth)
{
    QList<PageElement> elements;
    qreal indent = bq.level * 16.0;
    qreal innerWidth = availWidth - indent;

    for (const auto &child : bq.children) {
        std::visit([&](const auto &c) {
            using T = std::decay_t<decltype(c)>;
            if constexpr (std::is_same_v<T, Content::Paragraph>) {
                auto box = layoutParagraph(c, innerWidth);
                box.x += indent;
                box.hasBlockQuoteBorder = true;
                box.blockQuoteLevel = bq.level;
                box.blockQuoteIndent = indent;
                elements.append(box);
            } else if constexpr (std::is_same_v<T, Content::Heading>) {
                auto box = layoutHeading(c, innerWidth);
                box.x += indent;
                box.hasBlockQuoteBorder = true;
                box.blockQuoteLevel = bq.level;
                box.blockQuoteIndent = indent;
                elements.append(box);
            } else if constexpr (std::is_same_v<T, Content::CodeBlock>) {
                auto box = layoutCodeBlock(c, innerWidth);
                box.x += indent;
                box.hasBlockQuoteBorder = true;
                box.blockQuoteLevel = bq.level;
                box.blockQuoteIndent = indent;
                elements.append(box);
            } else if constexpr (std::is_same_v<T, Content::List>) {
                auto listElements = layoutList(c, availWidth - indent);
                for (auto &elem : listElements) {
                    if (auto *bb = std::get_if<BlockBox>(&elem)) {
                        bb->x += indent;
                        bb->hasBlockQuoteBorder = true;
                        bb->blockQuoteLevel = bq.level;
                        bb->blockQuoteIndent = indent;
                    }
                }
                elements.append(listElements);
            } else if constexpr (std::is_same_v<T, Content::Table>) {
                auto tbox = layoutTable(c, innerWidth);
                tbox.x += indent;
                elements.append(tbox);
            } else if constexpr (std::is_same_v<T, Content::HorizontalRule>) {
                auto box = layoutHorizontalRule(c, innerWidth);
                box.x += indent;
                box.hasBlockQuoteBorder = true;
                box.blockQuoteLevel = bq.level;
                box.blockQuoteIndent = indent;
                elements.append(box);
            } else if constexpr (std::is_same_v<T, Content::BlockQuote>) {
                // Nested blockquote
                auto nested = layoutBlockQuote(c, availWidth);
                elements.append(nested);
            }
        }, child);
    }

    return elements;
}

// --- List layout ---

QList<PageElement> Engine::layoutList(const Content::List &list, qreal availWidth, int depth)
{
    QList<PageElement> elements;
    qreal indent = 20.0 * (depth + 1);

    for (int i = 0; i < list.items.size(); ++i) {
        const auto &item = list.items[i];
        qreal itemBulletWidth = 0; // track for subsequent paragraphs

        for (const auto &child : item.children) {
            std::visit([&](const auto &c) {
                using T = std::decay_t<decltype(c)>;
                if constexpr (std::is_same_v<T, Content::Paragraph>) {
                    Content::Paragraph para = c;

                    bool isFirstPara = (&child == &item.children.first())
                                       && !para.inlines.isEmpty();
                    bool isTask = isFirstPara && item.isTask;

                    // Resolve body text style for bullet prefix sizing.
                    // Skip InlineCode to avoid inheriting monospace font.
                    Content::TextStyle firstStyle;
                    if (isFirstPara) {
                        for (const auto &inl : para.inlines) {
                            bool found = false;
                            std::visit([&](const auto &n) {
                                using U = std::decay_t<decltype(n)>;
                                if constexpr (std::is_same_v<U, Content::TextRun>
                                              || std::is_same_v<U, Content::Link>
                                              || std::is_same_v<U, Content::FootnoteRef>) {
                                    firstStyle = n.style;
                                    found = true;
                                }
                            }, inl);
                            if (found)
                                break;
                        }
                        if (firstStyle.fontFamily.isEmpty()) {
                            firstStyle.fontFamily = QStringLiteral("Noto Serif");
                            firstStyle.fontSize = 11.0;
                        }
                    }

                    int prefixLen = 0;
                    if (isFirstPara && !isTask) {
                        // Bullet/number prefix: measure width, then use hanging indent
                        QString prefix;
                        if (list.type == Content::ListType::Ordered)
                            prefix = QString::number(list.startNumber + i) + QStringLiteral(". ");
                        else
                            prefix = QStringLiteral("\u2022 ");
                        prefixLen = prefix.size();

                        Content::TextRun prefixRun;
                        prefixRun.text = prefix;
                        prefixRun.style = firstStyle;

                        QList<Content::InlineNode> prefixInlines;
                        prefixInlines.append(prefixRun);
                        itemBulletWidth = measureInlines(prefixInlines, firstStyle);

                        para.inlines.prepend(prefixRun);
                        para.format.leftMargin += indent + itemBulletWidth;
                        para.format.firstLineIndent = -itemBulletWidth;
                    } else if (isTask) {
                        // Task items: checkbox width as hanging indent
                        qreal cbWidth = firstStyle.fontSize * 0.85 + 3.0;
                        itemBulletWidth = cbWidth;
                        para.format.leftMargin += indent + cbWidth;
                        para.format.firstLineIndent = -cbWidth;
                    } else {
                        // Subsequent paragraph in same list item:
                        // align with continuation text (past bullet)
                        para.format.leftMargin += indent + itemBulletWidth;
                    }

                    auto box = layoutParagraph(para, availWidth);

                    // Mark bullet/number glyph boxes so justify skips them
                    if (prefixLen > 0 && !box.lines.isEmpty()) {
                        for (auto &gb : box.lines.first().glyphs) {
                            if (gb.textStart < prefixLen)
                                gb.isListMarker = true;
                            else
                                break;
                        }
                    }

                    if (isTask && !box.lines.isEmpty()) {
                        GlyphBox cbBox;
                        cbBox.checkboxState = item.taskChecked
                            ? GlyphBox::Checked : GlyphBox::Unchecked;
                        cbBox.fontSize = firstStyle.fontSize;
                        cbBox.style = firstStyle;
                        cbBox.width = itemBulletWidth;
                        cbBox.ascent = firstStyle.fontSize * 0.8;
                        cbBox.descent = firstStyle.fontSize * 0.2;
                        box.lines.first().glyphs.prepend(cbBox);
                        box.lines.first().width += itemBulletWidth;
                    }

                    elements.append(box);
                } else if constexpr (std::is_same_v<T, Content::List>) {
                    // Nested list
                    auto nested = layoutList(c, availWidth, depth + 1);
                    elements.append(nested);
                }
            }, child);
        }
    }

    return elements;
}

// --- Footnote section layout ---

FootnoteSectionBox Engine::layoutFootnoteSection(const Content::FootnoteSection &fs,
                                                  qreal availWidth)
{
    FootnoteSectionBox sbox;
    sbox.width = availWidth;
    sbox.showSeparator = fs.showSeparator;
    sbox.separatorLength = fs.separatorLength;

    qreal y = fs.showSeparator ? 12.0 : 0; // space for separator

    for (const auto &fn : fs.footnotes) {
        FootnoteBox fbox;
        fbox.label = fn.label;
        fbox.numberStyle = fn.numberStyle;
        fbox.y = y;

        // Layout footnote text
        Content::ParagraphFormat fmt;
        fmt.leftMargin = 20.0;
        fmt.firstLineIndent = -20.0;
        fbox.lines = breakIntoLines(fn.content, fn.textStyle, fmt, availWidth - 20.0);

        qreal h = 0;
        for (const auto &line : fbox.lines)
            h += line.height;
        fbox.height = h;

        sbox.footnotes.append(fbox);
        y += h + 2.0; // 2pt gap between footnotes
    }

    sbox.height = y;
    return sbox;
}

// --- Table splitting ---

QList<TableBox> Engine::splitTable(const TableBox &table, qreal availHeight, qreal pageHeight)
{
    QList<TableBox> slices;

    int headerRowCount = table.headerRowCount;

    // Calculate total header height
    qreal headerHeight = 0;
    for (int i = 0; i < headerRowCount && i < table.rows.size(); ++i)
        headerHeight += table.rows[i].height;

    // If headers alone > 50% of page, don't repeat them
    bool repeatHeaders = (headerHeight <= pageHeight * 0.5);
    int effectiveHeaderCount = repeatHeaders ? headerRowCount : 0;
    qreal effectiveHeaderHeight = repeatHeaders ? headerHeight : 0;

    // Helper: create a new slice with table metadata and (optionally) header rows
    auto newSlice = [&]() -> TableBox {
        TableBox slice;
        slice.width = table.width;
        slice.borderWidth = table.borderWidth;
        slice.borderColor = table.borderColor;
        slice.innerBorderWidth = table.innerBorderWidth;
        slice.innerBorderColor = table.innerBorderColor;
        slice.headerBottomBorderWidth = table.headerBottomBorderWidth;
        slice.headerBottomBorderColor = table.headerBottomBorderColor;
        slice.cellPadding = table.cellPadding;
        slice.columnPositions = table.columnPositions;
        slice.headerRowCount = effectiveHeaderCount;
        slice.source = table.source;

        // Copy header rows with re-based y positions
        qreal hy = 0;
        for (int i = 0; i < effectiveHeaderCount && i < table.rows.size(); ++i) {
            TableRowBox row = table.rows[i];
            row.y = hy;
            for (auto &cell : row.cells)
                cell.y = hy;
            slice.rows.append(row);
            hy += row.height;
        }
        return slice;
    };

    TableBox currentSlice = newSlice();
    qreal currentHeight = effectiveHeaderHeight;
    qreal currentAvail = availHeight;
    int bodyRowsInSlice = 0;

    for (int i = headerRowCount; i < table.rows.size(); ++i) {
        qreal rowHeight = table.rows[i].height;

        // Need to start a new slice? Only if we have body rows in the current one
        if (currentHeight + rowHeight > currentAvail && bodyRowsInSlice > 0) {
            currentSlice.height = currentHeight;
            slices.append(currentSlice);

            currentSlice = newSlice();
            currentHeight = effectiveHeaderHeight;
            currentAvail = pageHeight;
            bodyRowsInSlice = 0;
        }

        // Copy body row with re-based y position
        TableRowBox row = table.rows[i];
        row.y = currentHeight;
        for (auto &cell : row.cells)
            cell.y = currentHeight;
        currentSlice.rows.append(row);
        currentHeight += rowHeight;
        bodyRowsInSlice++;
    }

    // Finalize last slice
    if (bodyRowsInSlice > 0) {
        currentSlice.height = currentHeight;
        slices.append(currentSlice);
    }

    return slices;
}

// --- Page assignment ---

void Engine::assignToPages(const QList<PageElement> &elements,
                            const PageLayout &pageLayout,
                            LayoutResult &result)
{
    QSizeF contentSize = pageLayout.contentSizePoints();
    qreal pageHeight = contentSize.height();

    Page currentPage;
    currentPage.pageNumber = 0;
    qreal y = 0;

    for (int idx = 0; idx < elements.size(); ++idx) {
        const auto &element = elements[idx];

        // Handle tables separately for page-splitting
        if (auto *tablePtr = std::get_if<TableBox>(&element)) {
            const TableBox &table = *tablePtr;
            qreal remaining = pageHeight - y;

            if (table.height <= remaining || currentPage.elements.isEmpty()) {
                TableBox positioned = table;
                positioned.y = y;
                currentPage.elements.append(positioned);
                y += table.height;
            } else {
                auto slices = splitTable(table, remaining, pageHeight);
                for (int si = 0; si < slices.size(); ++si) {
                    if (si > 0) {
                        currentPage.contentHeight = y;
                        result.pages.append(currentPage);
                        currentPage = Page{};
                        currentPage.pageNumber = result.pages.size();
                        y = 0;
                    }
                    slices[si].y = y;
                    currentPage.elements.append(slices[si]);
                    y += slices[si].height;
                }
            }
            continue;
        }

        // Non-table elements: blocks and footnote sections
        qreal elementHeight = 0;
        qreal spaceBefore = 0;
        qreal spaceAfter = 0;
        bool keepWithNext = false;
        int lineCount = 0;

        std::visit([&](const auto &e) {
            using T = std::decay_t<decltype(e)>;
            if constexpr (std::is_same_v<T, BlockBox>) {
                elementHeight = e.height;
                spaceBefore = e.spaceBefore;
                spaceAfter = e.spaceAfter;
                keepWithNext = e.keepWithNext;
                lineCount = e.lines.size();
            } else if constexpr (std::is_same_v<T, FootnoteSectionBox>) {
                elementHeight = e.height;
                spaceBefore = 20.0;
            }
        }, element);

        qreal totalHeight = spaceBefore + elementHeight + spaceAfter;

        // Page break needed?
        bool needsPageBreak = (y + totalHeight > pageHeight) && !currentPage.elements.isEmpty();

        // Keep-with-next: if this is a heading (or element with keepWithNext),
        // peek at the next element. If both won't fit, break before this one.
        if (keepWithNext && !needsPageBreak && idx + 1 < elements.size()
            && !currentPage.elements.isEmpty()) {
            qreal nextHeight = 0;
            std::visit([&](const auto &ne) {
                using T = std::decay_t<decltype(ne)>;
                if constexpr (std::is_same_v<T, BlockBox>) {
                    nextHeight = ne.spaceBefore + ne.height;
                    // Only need the first couple of lines to keep with heading
                    if (ne.lines.size() > 2) {
                        qreal twoLineH = 0;
                        for (int li = 0; li < 2 && li < ne.lines.size(); ++li)
                            twoLineH += ne.lines[li].height;
                        nextHeight = ne.spaceBefore + twoLineH;
                    }
                } else if constexpr (std::is_same_v<T, TableBox>) {
                    nextHeight = ne.height;
                } else if constexpr (std::is_same_v<T, FootnoteSectionBox>) {
                    nextHeight = 20.0 + ne.height;
                }
            }, elements[idx + 1]);

            if (y + totalHeight + nextHeight > pageHeight)
                needsPageBreak = true;
        }

        // Orphan protection: if a multi-line paragraph would have fewer than
        // 2 lines on the current page, push the whole paragraph to the next page.
        if (!needsPageBreak && lineCount > 2 && !currentPage.elements.isEmpty()) {
            // Calculate how many lines fit
            qreal remaining = pageHeight - y - spaceBefore;
            int linesFitting = 0;
            qreal accum = 0;
            // Access lines via the block box
            if (auto *bb = std::get_if<BlockBox>(&element)) {
                for (const auto &line : bb->lines) {
                    accum += line.height;
                    if (accum <= remaining)
                        linesFitting++;
                    else
                        break;
                }
            }
            if (linesFitting > 0 && linesFitting < 2)
                needsPageBreak = true;
        }

        if (needsPageBreak) {
            currentPage.contentHeight = y;
            result.pages.append(currentPage);
            currentPage = Page{};
            currentPage.pageNumber = result.pages.size();
            y = 0;
        }

        y += spaceBefore;

        // Set element position
        PageElement positioned = element;
        std::visit([&](auto &e) {
            using T = std::decay_t<decltype(e)>;
            if constexpr (std::is_same_v<T, BlockBox>) {
                e.y = y;
            } else if constexpr (std::is_same_v<T, FootnoteSectionBox>) {
                e.y = y;
            }
        }, positioned);

        currentPage.elements.append(positioned);
        y += elementHeight + spaceAfter;
    }

    // Don't forget the last page
    if (!currentPage.elements.isEmpty()) {
        currentPage.contentHeight = y;
        result.pages.append(currentPage);
    }

    // Ensure at least one empty page
    if (result.pages.isEmpty()) {
        result.pages.append(Page{});
    }
}

qreal Engine::measureInlines(const QList<Content::InlineNode> &inlines,
                              const Content::TextStyle &baseStyle)
{
    auto collected = collectInlines(inlines, baseStyle);
    if (collected.text.isEmpty())
        return 0;

    QList<ShapedRun> runs = m_textShaper->shape(collected.text, collected.styleRuns);
    qreal width = 0;
    for (const auto &run : runs)
        for (const auto &g : run.glyphs)
            width += g.xAdvance;
    return width;
}

} // namespace Layout
