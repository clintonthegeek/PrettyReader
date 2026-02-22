/*
 * boxtreerenderer.cpp — Base class for box tree rendering backends
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "boxtreerenderer.h"

BoxTreeRenderer::BoxTreeRenderer(FontManager *fontManager)
    : m_fontManager(fontManager)
{
}

BoxTreeRenderer::~BoxTreeRenderer() = default;

// --- Traversal stubs (populated in Task 2) ---

void BoxTreeRenderer::renderElement(const Layout::PageElement &) {}
void BoxTreeRenderer::renderBlockBox(const Layout::BlockBox &) {}
void BoxTreeRenderer::renderTableBox(const Layout::TableBox &) {}
void BoxTreeRenderer::renderFootnoteSectionBox(const Layout::FootnoteSectionBox &) {}
void BoxTreeRenderer::renderLineBox(const Layout::LineBox &, qreal, qreal, qreal) {}
void BoxTreeRenderer::renderGlyphBox(const Layout::GlyphBox &, qreal, qreal) {}
void BoxTreeRenderer::renderHersheyGlyphBox(const Layout::GlyphBox &, qreal, qreal) {}
void BoxTreeRenderer::renderGlyphDecorations(const Layout::GlyphBox &, qreal, qreal, qreal) {}
void BoxTreeRenderer::renderCheckbox(const Layout::GlyphBox &, qreal, qreal) {}
void BoxTreeRenderer::renderImageBlock(const Layout::BlockBox &) {}

// --- Shared justification helpers ---

JustifyParams BoxTreeRenderer::computeJustification(const Layout::LineBox &line,
                                                    qreal availWidth,
                                                    qreal maxJustifyGap) const
{
    JustifyParams result;

    if (line.alignment != Qt::AlignJustify || line.isLastLine
        || line.glyphs.size() <= 1 || line.width >= availWidth)
        return result;

    if (line.justify.wordGapCount > 0) {
        // New path: use pre-computed JustifyInfo from the layout engine
        result.doJustify = true;
        result.extraPerGap = line.justify.extraWordSpacing;
        result.extraPerChar = line.justify.extraLetterSpacing;
        return result;
    }

    // Legacy fallback: compute gaps inline
    int gapCount = 0;
    for (int i = 1; i < line.glyphs.size(); ++i) {
        if (line.glyphs[i].startsAfterSoftHyphen)
            continue;
        if (line.glyphs[i].attachedToPrevious)
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
        if (epg <= maxJustifyGap) {
            result.doJustify = true;
            result.extraPerGap = epg;
        }
    }

    return result;
}

QList<qreal> BoxTreeRenderer::computeGlyphXPositions(const Layout::LineBox &line,
                                                     qreal originX,
                                                     qreal availWidth) const
{
    JustifyParams jp = computeJustification(line, availWidth);
    QList<qreal> positions;
    positions.reserve(line.glyphs.size());

    if (jp.doJustify) {
        qreal cx = originX;
        for (int i = 0; i < line.glyphs.size(); ++i) {
            positions.append(cx);
            cx += line.glyphs[i].width;
            if (i < line.glyphs.size() - 1)
                cx += jp.extraPerChar * line.glyphs[i].glyphs.size();
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
                    cx += jp.extraPerGap;
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
            positions.append(cx);
            cx += line.glyphs[i].width;
        }
    }

    return positions;
}
