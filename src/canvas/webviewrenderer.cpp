/*
 * webviewrenderer.cpp — QPainter rendering of Layout box tree
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "webviewrenderer.h"
#include "fontmanager.h"
#include "hersheyfont.h"

#include <QGlyphRun>
#include <QPainter>
#include <QPen>
#include <QRawFont>

WebViewRenderer::WebViewRenderer(FontManager *fontManager)
    : m_fontManager(fontManager)
{
}

const QRawFont &WebViewRenderer::rawFontFor(FontFace *face, qreal sizePoints)
{
    int sizeKey = qRound(sizePoints * 100);
    auto key = qMakePair(face, sizeKey);
    auto it = m_rawFontCache.find(key);
    if (it != m_rawFontCache.end())
        return it.value();

    QByteArray data = m_fontManager->rawFontData(face);
    auto inserted = m_rawFontCache.insert(key, QRawFont(data, sizePoints));
    return inserted.value();
}

// --- Block rendering ---

void WebViewRenderer::renderBlockBox(QPainter *painter, const Layout::BlockBox &box)
{
    // Background
    if (box.background.isValid()) {
        painter->save();
        painter->setPen(Qt::NoPen);
        painter->setBrush(box.background);
        QRectF bgRect(box.x - box.padding, box.y - box.padding,
                       box.width + box.padding * 2, box.height + box.padding * 2);
        painter->drawRect(bgRect);

        // Border
        if (box.borderWidth > 0 && box.borderColor.isValid()) {
            painter->setPen(QPen(box.borderColor, box.borderWidth));
            painter->setBrush(Qt::NoBrush);
            painter->drawRect(bgRect);
        }
        painter->restore();
    }

    // Image block
    if (box.type == Layout::BlockBox::ImageBlock) {
        renderImageBlock(painter, box);
        return;
    }

    // Horizontal rule
    if (box.type == Layout::BlockBox::HRuleBlock) {
        painter->save();
        painter->setPen(QPen(QColor(204, 204, 204), 0.5));
        qreal ruleY = box.y + box.height / 2;
        painter->drawLine(QPointF(box.x, ruleY), QPointF(box.x + box.width, ruleY));
        painter->restore();
        return;
    }

    // Blockquote left border
    if (box.hasBlockQuoteBorder && box.blockQuoteLevel > 0) {
        painter->save();
        painter->setPen(QPen(QColor(204, 204, 204), 2.0));
        qreal borderX = box.blockQuoteIndent - 8.0;
        qreal borderTop = box.y - box.spaceBefore;
        qreal borderBottom = box.y + box.height + box.spaceAfter;
        painter->drawLine(QPointF(borderX, borderTop), QPointF(borderX, borderBottom));
        painter->restore();
    }

    // Lines
    qreal lineY = 0;
    for (int li = 0; li < box.lines.size(); ++li) {
        qreal lineX = box.x;
        qreal lineAvailWidth = box.width;
        if (li == 0 && box.firstLineIndent != 0) {
            lineX += box.firstLineIndent;
            lineAvailWidth -= box.firstLineIndent;
        }
        renderLineBox(painter, box.lines[li], lineX, box.y + lineY, lineAvailWidth);
        lineY += box.lines[li].height;
    }
}

// --- Table rendering ---

void WebViewRenderer::renderTableBox(QPainter *painter, const Layout::TableBox &box)
{
    qreal tableLeft = box.x;
    qreal tableTop = box.y;

    // === Pass 1: Cell backgrounds ===
    for (const auto &row : box.rows) {
        for (const auto &cell : row.cells) {
            if (cell.background.isValid()) {
                painter->save();
                painter->setPen(Qt::NoPen);
                painter->setBrush(cell.background);
                qreal cellX = tableLeft + cell.x;
                qreal cellY = tableTop + cell.y;
                painter->drawRect(QRectF(cellX, cellY, cell.width, cell.height));
                painter->restore();
            }
        }
    }

    // === Pass 2: Cell content ===
    for (const auto &row : box.rows) {
        for (const auto &cell : row.cells) {
            qreal cellX = tableLeft + cell.x;
            qreal cellY = tableTop + cell.y;
            qreal innerX = cellX + box.cellPadding;
            qreal innerY = cellY + box.cellPadding;
            qreal lineY = 0;
            for (const auto &line : cell.lines) {
                renderLineBox(painter, line, innerX, innerY + lineY,
                              cell.width - box.cellPadding * 2);
                lineY += line.height;
            }
        }
    }

    // === Pass 3: Grid borders ===
    painter->save();

    // Inner horizontal lines (between rows)
    if (box.innerBorderWidth > 0 && box.innerBorderColor.isValid()) {
        painter->setPen(QPen(box.innerBorderColor, box.innerBorderWidth));
        qreal rowY = 0;
        for (int ri = 0; ri < box.rows.size() - 1; ++ri) {
            rowY += box.rows[ri].height;
            // Skip header-bottom line (drawn separately with heavier weight)
            if (ri == box.headerRowCount - 1)
                continue;
            qreal lineY = tableTop + rowY;
            painter->drawLine(QPointF(tableLeft, lineY),
                              QPointF(tableLeft + box.width, lineY));
        }
    }

    // Inner vertical lines (between columns)
    if (box.innerBorderWidth > 0 && box.innerBorderColor.isValid()
        && box.columnPositions.size() > 2) {
        painter->setPen(QPen(box.innerBorderColor, box.innerBorderWidth));
        qreal tableBottom = tableTop + box.height;
        for (int ci = 1; ci < box.columnPositions.size() - 1; ++ci) {
            qreal lineX = tableLeft + box.columnPositions[ci];
            painter->drawLine(QPointF(lineX, tableTop),
                              QPointF(lineX, tableBottom));
        }
    }

    // Header bottom border (heavier line under header row)
    if (box.headerRowCount > 0
        && box.headerBottomBorderWidth > 0
        && box.headerBottomBorderColor.isValid()) {
        qreal headerHeight = 0;
        for (int ri = 0; ri < box.headerRowCount && ri < box.rows.size(); ++ri)
            headerHeight += box.rows[ri].height;
        qreal hbY = tableTop + headerHeight;
        painter->setPen(QPen(box.headerBottomBorderColor, box.headerBottomBorderWidth));
        painter->drawLine(QPointF(tableLeft, hbY),
                          QPointF(tableLeft + box.width, hbY));
    }

    // Outer border (on top of everything)
    if (box.borderWidth > 0 && box.borderColor.isValid()) {
        painter->setPen(QPen(box.borderColor, box.borderWidth));
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(QRectF(tableLeft, tableTop, box.width, box.height));
    }

    painter->restore();
}

// --- Footnote section rendering ---

void WebViewRenderer::renderFootnoteSectionBox(QPainter *painter,
                                                const Layout::FootnoteSectionBox &box)
{
    qreal sectionY = box.y;

    // Separator line
    if (box.showSeparator) {
        painter->save();
        painter->setPen(QPen(QColor(179, 179, 179), 0.5)); // ~0.7 gray
        qreal sepWidth = box.width * box.separatorLength;
        painter->drawLine(QPointF(box.x, sectionY), QPointF(box.x + sepWidth, sectionY));
        painter->restore();
    }

    for (const auto &fn : box.footnotes) {
        qreal fnY = sectionY + fn.y;
        qreal lineY = 0;
        for (const auto &line : fn.lines) {
            renderLineBox(painter, line, box.x, fnY + lineY, box.width);
            lineY += line.height;
        }
    }
}

// --- Line rendering ---

void WebViewRenderer::renderLineBox(QPainter *painter, const Layout::LineBox &line,
                                     qreal originX, qreal originY, qreal availWidth)
{
    qreal baselineY = originY + line.baseline;

    // Justification
    bool doJustify = false;
    qreal extraPerGap = 0;
    qreal extraPerChar = 0;

    if (line.alignment == Qt::AlignJustify && !line.isLastLine
        && line.glyphs.size() > 1 && line.width < availWidth) {
        if (line.justify.wordGapCount > 0) {
            doJustify = true;
            extraPerGap = line.justify.extraWordSpacing;
            extraPerChar = line.justify.extraLetterSpacing;
        } else {
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
                if (epg <= 20.0) {
                    doJustify = true;
                    extraPerGap = epg;
                }
            }
        }
    }

    qreal x;

    if (doJustify) {
        x = originX;
        for (int i = 0; i < line.glyphs.size(); ++i) {
            renderGlyphBox(painter, line.glyphs[i], x, baselineY);
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
            renderGlyphBox(painter, gbox, x, baselineY);
            x += gbox.width;
        }
    }

    // Trailing soft-hyphen
    if (line.showTrailingHyphen && !line.glyphs.isEmpty()) {
        const auto &lastGbox = line.glyphs.last();
        if (lastGbox.font && !lastGbox.font->isHershey) {
            QRawFont rf = rawFontFor(lastGbox.font, lastGbox.fontSize);
            if (rf.isValid() && lastGbox.font->ftFace) {
                quint32 hyphenGid = FT_Get_Char_Index(lastGbox.font->ftFace, '-');
                if (hyphenGid > 0) {
                    QGlyphRun gr;
                    gr.setRawFont(rf);
                    gr.setGlyphIndexes({hyphenGid});
                    gr.setPositions({QPointF(0, 0)});
                    painter->save();
                    painter->setPen(lastGbox.style.foreground);
                    painter->drawGlyphRun(QPointF(x, baselineY), gr);
                    painter->restore();
                }
            }
        } else if (lastGbox.font && lastGbox.font->isHershey && lastGbox.font->hersheyFont) {
            HersheyFont *hFont = lastGbox.font->hersheyFont;
            const HersheyGlyph *hGlyph = hFont->glyph(U'-');
            if (hGlyph) {
                qreal scale = lastGbox.fontSize / hFont->unitsPerEm();
                painter->save();
                painter->setPen(QPen(lastGbox.style.foreground,
                                     0.02 * lastGbox.fontSize * (lastGbox.font->hersheyBold ? 1.8 : 1.0),
                                     Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
                painter->setBrush(Qt::NoBrush);
                QTransform t;
                if (lastGbox.font->hersheyItalic)
                    t = QTransform(scale, 0, -scale * 0.2126, scale, x, baselineY);
                else
                    t = QTransform(scale, 0, 0, scale, x, baselineY);
                painter->setTransform(t, true);
                for (const auto &stroke : hGlyph->strokes) {
                    if (stroke.size() < 2) continue;
                    QPolygonF poly;
                    for (const auto &pt : stroke)
                        poly << QPointF(pt.x() - hGlyph->leftBound, -pt.y());
                    painter->drawPolyline(poly);
                }
                painter->restore();
            }
        }
    }
}

// --- Glyph rendering (TTF via QRawFont + QGlyphRun) ---

void WebViewRenderer::renderGlyphBox(QPainter *painter, const Layout::GlyphBox &gbox,
                                      qreal x, qreal baselineY)
{
    if (gbox.font && gbox.font->isHershey) {
        renderHersheyGlyphBox(painter, gbox, x, baselineY);
        return;
    }

    if (gbox.checkboxState != Layout::GlyphBox::NoCheckbox) {
        renderCheckbox(painter, gbox, x, baselineY);
        return;
    }

    if (gbox.glyphs.isEmpty() || !gbox.font)
        return;

    // Inline background
    if (gbox.style.background.isValid()) {
        painter->save();
        painter->setPen(Qt::NoPen);
        painter->setBrush(gbox.style.background);
        painter->drawRect(QRectF(x - 1, baselineY - gbox.ascent - 1,
                                  gbox.width + 2, gbox.ascent + gbox.descent + 2));
        painter->restore();
    }

    QRawFont rf = rawFontFor(gbox.font, gbox.fontSize);
    if (!rf.isValid())
        return;

    QVector<quint32> glyphIds;
    QVector<QPointF> positions;
    glyphIds.reserve(gbox.glyphs.size());
    positions.reserve(gbox.glyphs.size());

    qreal curX = 0;
    for (const auto &g : gbox.glyphs) {
        glyphIds.append(g.glyphId);
        qreal gx = curX + g.xOffset;
        qreal gy = -g.yOffset; // QPainter Y is top-down
        if (gbox.style.superscript)
            gy -= gbox.fontSize * 0.35;
        else if (gbox.style.subscript)
            gy += gbox.fontSize * 0.15;
        positions.append(QPointF(gx, gy));
        curX += g.xAdvance;
    }

    QGlyphRun glyphRun;
    glyphRun.setRawFont(rf);
    glyphRun.setGlyphIndexes(glyphIds);
    glyphRun.setPositions(positions);

    painter->save();
    painter->setPen(gbox.style.foreground);
    painter->drawGlyphRun(QPointF(x, baselineY), glyphRun);
    painter->restore();

    renderGlyphDecorations(painter, gbox, x, baselineY, x + gbox.width);
}

// --- Hershey glyph rendering (stroked polylines) ---

void WebViewRenderer::renderHersheyGlyphBox(QPainter *painter, const Layout::GlyphBox &gbox,
                                              qreal x, qreal baselineY)
{
    if (gbox.glyphs.isEmpty() || !gbox.font || !gbox.font->hersheyFont)
        return;

    HersheyFont *hFont = gbox.font->hersheyFont;
    qreal fontSize = gbox.fontSize;
    qreal scale = fontSize / hFont->unitsPerEm();

    // Inline background
    if (gbox.style.background.isValid()) {
        painter->save();
        painter->setPen(Qt::NoPen);
        painter->setBrush(gbox.style.background);
        painter->drawRect(QRectF(x - 1, baselineY - gbox.ascent - 1,
                                  gbox.width + 2, gbox.ascent + gbox.descent + 2));
        painter->restore();
    }

    qreal strokeWidth = 0.02 * fontSize;
    if (gbox.font->hersheyBold)
        strokeWidth *= 1.8;

    painter->save();
    painter->setPen(QPen(gbox.style.foreground, strokeWidth,
                          Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter->setBrush(Qt::NoBrush);
    QTransform baseTransform = painter->transform();

    qreal curX = x;
    for (const auto &g : gbox.glyphs) {
        const HersheyGlyph *hGlyph = hFont->glyph(static_cast<char32_t>(g.glyphId));
        if (!hGlyph) {
            curX += g.xAdvance;
            continue;
        }

        qreal gx = curX + g.xOffset;
        qreal gy = baselineY - g.yOffset;
        if (gbox.style.superscript)
            gy -= fontSize * 0.35;
        else if (gbox.style.subscript)
            gy += fontSize * 0.15;

        QTransform t;
        if (gbox.font->hersheyItalic)
            t = QTransform(scale, 0, -scale * 0.2126, scale, gx, gy);
        else
            t = QTransform(scale, 0, 0, scale, gx, gy);
        painter->setTransform(baseTransform * t, false);

        for (const auto &stroke : hGlyph->strokes) {
            if (stroke.size() < 2) continue;
            QPolygonF poly;
            for (const auto &pt : stroke)
                poly << QPointF(pt.x() - hGlyph->leftBound, -pt.y());
            painter->drawPolyline(poly);
        }

        curX += g.xAdvance;
    }
    painter->restore();

    renderGlyphDecorations(painter, gbox, x, baselineY, curX);
}

// --- Decorations ---

void WebViewRenderer::renderGlyphDecorations(QPainter *painter, const Layout::GlyphBox &gbox,
                                               qreal x, qreal baselineY, qreal endX)
{
    if (gbox.style.underline) {
        painter->save();
        painter->setPen(QPen(gbox.style.foreground, 0.5));
        qreal uy = baselineY + gbox.descent * 0.3;
        painter->drawLine(QPointF(x, uy), QPointF(endX, uy));
        painter->restore();
    }

    if (gbox.style.strikethrough) {
        painter->save();
        painter->setPen(QPen(gbox.style.foreground, 0.5));
        qreal sy = baselineY - gbox.ascent * 0.3;
        painter->drawLine(QPointF(x, sy), QPointF(endX, sy));
        painter->restore();
    }

    if (!gbox.style.linkHref.isEmpty()) {
        m_linkHitRects.append({QRectF(x, baselineY - gbox.ascent,
                                       endX - x, gbox.ascent + gbox.descent),
                                gbox.style.linkHref});
    }
}

// --- Checkbox ---

void WebViewRenderer::renderCheckbox(QPainter *painter, const Layout::GlyphBox &gbox,
                                      qreal x, qreal baselineY)
{
    qreal size = gbox.fontSize * 0.7;
    qreal r = size * 0.12;
    qreal lw = size * 0.07;
    qreal cx = x + 1.0;
    qreal cy = baselineY - size * 0.75;

    QRectF boxRect(cx, cy, size, size);
    QColor strokeColor = gbox.style.foreground.isValid() ? gbox.style.foreground : QColor(0x33, 0x33, 0x33);

    painter->save();
    if (gbox.checkboxState == Layout::GlyphBox::Checked) {
        painter->setPen(QPen(strokeColor, lw));
        painter->setBrush(QColor(235, 242, 255));
        painter->drawRoundedRect(boxRect, r, r);

        QPen checkPen(strokeColor, lw * 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        painter->setPen(checkPen);
        painter->setBrush(Qt::NoBrush);
        QPolygonF check;
        check << QPointF(cx + size * 0.20, cy + size * 0.50)
              << QPointF(cx + size * 0.42, cy + size * 0.75)
              << QPointF(cx + size * 0.82, cy + size * 0.22);
        painter->drawPolyline(check);
    } else {
        painter->setPen(QPen(strokeColor, lw));
        painter->setBrush(Qt::NoBrush);
        painter->drawRoundedRect(boxRect, r, r);
    }
    painter->restore();
}

// --- Image ---

void WebViewRenderer::renderImageBlock(QPainter *painter, const Layout::BlockBox &box)
{
    if (box.image.isNull())
        return;

    QRectF imgRect(box.x, box.y, box.imageWidth, box.imageHeight);
    painter->drawImage(imgRect, box.image);
}
