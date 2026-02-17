/*
 * linebreaker.cpp — Knuth-Plass optimal line breaking
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "linebreaker.h"

#include <cmath>

namespace LineBreaking {

BreakResult findBreaks(const QList<Item> &items,
                       const QList<qreal> &lineWidths,
                       const Config &config)
{
    Q_UNUSED(items); Q_UNUSED(lineWidths); Q_UNUSED(config);
    BreakResult result;
    result.optimal = false;
    return result;
}

BreakResult findBreaksTiered(const QList<Item> &items,
                             const QList<qreal> &lineWidths,
                             const Config &baseConfig)
{
    Q_UNUSED(items); Q_UNUSED(lineWidths); Q_UNUSED(baseConfig);
    BreakResult result;
    result.optimal = false;
    return result;
}

qreal computeAdjustmentRatio(const QList<Item> &items,
                              int start, int end,
                              qreal lineWidth)
{
    Q_UNUSED(items); Q_UNUSED(start); Q_UNUSED(end); Q_UNUSED(lineWidth);
    return 0;
}

BlendedSpacing computeBlendedSpacing(qreal adjustmentRatio,
                                      qreal naturalWordGlueWidth,
                                      int wordGapCount,
                                      int charCount,
                                      qreal fontSize,
                                      const Config &config)
{
    Q_UNUSED(adjustmentRatio); Q_UNUSED(naturalWordGlueWidth);
    Q_UNUSED(wordGapCount); Q_UNUSED(charCount);
    Q_UNUSED(fontSize); Q_UNUSED(config);
    return {};
}

} // namespace LineBreaking
