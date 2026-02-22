/*
 * webviewrenderer.h — QPainter-based rendering of Layout box tree
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef PRETTYREADER_WEBVIEWRENDERER_H
#define PRETTYREADER_WEBVIEWRENDERER_H

#include "layoutengine.h"

#include <QHash>
#include <QRawFont>

class FontManager;
class QPainter;

struct LinkHitRect {
    QRectF rect;
    QString href;
};

class WebViewRenderer
{
public:
    explicit WebViewRenderer(FontManager *fontManager);

    void renderBlockBox(QPainter *painter, const Layout::BlockBox &box);
    void renderTableBox(QPainter *painter, const Layout::TableBox &box);
    void renderFootnoteSectionBox(QPainter *painter, const Layout::FootnoteSectionBox &box);

    const QList<LinkHitRect> &linkHitRects() const { return m_linkHitRects; }
    void clearLinkHitRects() { m_linkHitRects.clear(); }

private:
    void renderLineBox(QPainter *painter, const Layout::LineBox &line,
                       qreal originX, qreal originY, qreal availWidth);
    void renderGlyphBox(QPainter *painter, const Layout::GlyphBox &gbox,
                        qreal x, qreal baselineY);
    void renderHersheyGlyphBox(QPainter *painter, const Layout::GlyphBox &gbox,
                                qreal x, qreal baselineY);
    void renderGlyphDecorations(QPainter *painter, const Layout::GlyphBox &gbox,
                                 qreal x, qreal baselineY, qreal endX);
    void renderCheckbox(QPainter *painter, const Layout::GlyphBox &gbox,
                        qreal x, qreal baselineY);
    void renderImageBlock(QPainter *painter, const Layout::BlockBox &box);

    const QRawFont &rawFontFor(FontFace *face, qreal sizePoints);

    FontManager *m_fontManager;
    QHash<QPair<FontFace *, int>, QRawFont> m_rawFontCache; // key: (face, size*100)
    QList<LinkHitRect> m_linkHitRects;
};

#endif // PRETTYREADER_WEBVIEWRENDERER_H
