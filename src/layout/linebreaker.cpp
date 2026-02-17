/*
 * linebreaker.cpp — Knuth-Plass optimal line breaking
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "linebreaker.h"

#include <cmath>
#include <limits>

namespace LineBreaking {

// ---------------------------------------------------------------------------
// Helper: classify adjustment ratio into a fitness class
// ---------------------------------------------------------------------------
static FitnessClass fitnessClassFromRatio(qreal r)
{
    if (r < -0.5)
        return Tight;
    if (r <= 0.5)
        return Normal;
    if (r <= 1.0)
        return Loose;
    return VeryLoose;
}

// ---------------------------------------------------------------------------
// Helper: look up the target line width for a given line number
// ---------------------------------------------------------------------------
static qreal lineWidthForLine(const QList<qreal> &lineWidths, int lineNumber)
{
    if (lineNumber < lineWidths.size())
        return lineWidths[lineNumber];
    return lineWidths.last();
}

// ---------------------------------------------------------------------------
// computeAdjustmentRatio
// ---------------------------------------------------------------------------
qreal computeAdjustmentRatio(const QList<Item> &items,
                              int start, int end,
                              qreal lineWidth)
{
    if (start >= end || items.isEmpty())
        return 0;

    constexpr qreal INF = std::numeric_limits<qreal>::infinity();

    qreal totalWidth = 0;
    qreal totalStretch = 0;
    qreal totalShrink = 0;

    // Skip leading glue right after the break
    int first = start;
    if (first < end && items[first].type == Item::GlueType)
        ++first;

    for (int i = first; i < end; ++i) {
        const auto &item = items[i];
        switch (item.type) {
        case Item::BoxType:
            totalWidth += item.box.width;
            break;
        case Item::GlueType:
            totalWidth += item.glue.width;
            totalStretch += item.glue.stretch;
            totalShrink += item.glue.shrink;
            break;
        case Item::PenaltyType:
            // Penalty width only counts if we break here (at position end-1)
            break;
        }
    }

    // If breaking at a penalty, add its width (e.g. hyphen width)
    if (end > 0 && end <= items.size()) {
        int breakIdx = end - 1;
        if (breakIdx >= 0 && items[breakIdx].type == Item::PenaltyType)
            totalWidth += items[breakIdx].penalty.width;
    }

    qreal shortfall = lineWidth - totalWidth;

    if (std::abs(shortfall) < 1e-10)
        return 0;

    if (shortfall > 0) {
        if (totalStretch > 0)
            return shortfall / totalStretch;
        return INF;
    }

    // shortfall < 0
    if (totalShrink > 0)
        return shortfall / totalShrink;
    return -INF;
}

// ---------------------------------------------------------------------------
// Internal node for the DP active-node list
// ---------------------------------------------------------------------------
namespace {

struct Node {
    int itemIndex = 0;
    int lineNumber = 0;
    FitnessClass fitness = Normal;
    qreal totalWidth = 0;
    qreal totalStretch = 0;
    qreal totalShrink = 0;
    qreal totalDemerits = 0;
    int prevNode = -1;
    bool flagged = false;
};

} // anonymous namespace

// ---------------------------------------------------------------------------
// findBreaks — core Knuth-Plass DP
// ---------------------------------------------------------------------------
BreakResult findBreaks(const QList<Item> &items,
                       const QList<qreal> &lineWidths,
                       const Config &config)
{
    BreakResult result;
    result.optimal = true;

    if (items.isEmpty() || lineWidths.isEmpty()) {
        result.optimal = false;
        return result;
    }

    const int n = items.size();
    constexpr qreal INF = std::numeric_limits<qreal>::infinity();

    // Build prefix sums of width, stretch, shrink
    QList<qreal> prefixWidth(n + 1, 0);
    QList<qreal> prefixStretch(n + 1, 0);
    QList<qreal> prefixShrink(n + 1, 0);

    for (int i = 0; i < n; ++i) {
        prefixWidth[i + 1] = prefixWidth[i];
        prefixStretch[i + 1] = prefixStretch[i];
        prefixShrink[i + 1] = prefixShrink[i];

        switch (items[i].type) {
        case Item::BoxType:
            prefixWidth[i + 1] += items[i].box.width;
            break;
        case Item::GlueType:
            prefixWidth[i + 1] += items[i].glue.width;
            prefixStretch[i + 1] += items[i].glue.stretch;
            prefixShrink[i + 1] += items[i].glue.shrink;
            break;
        case Item::PenaltyType:
            // Penalty widths are not part of the normal flow;
            // they only appear if we break at them.
            break;
        }
    }

    // Helper: compute prefix sums AFTER skipping leading glue past a break at
    // position `breakIdx`. This gives us the "start-of-next-line" totals.
    auto skipGlueAfter = [&](int breakIdx, qreal &w, qreal &st, qreal &sh) {
        int pos = breakIdx + 1;
        // Skip glue items that appear at the start of the next line
        while (pos < n && items[pos].type == Item::GlueType)
            ++pos;
        w = prefixWidth[pos];
        st = prefixStretch[pos];
        sh = prefixShrink[pos];
    };

    // All nodes ever created (indexed by integer)
    QList<Node> nodes;
    // Active node indices
    QList<int> active;

    // Seed: a virtual break at position 0 (before first item)
    {
        Node seed;
        seed.itemIndex = 0;
        seed.lineNumber = 0;
        seed.fitness = Normal;
        seed.totalDemerits = 0;
        seed.flagged = false;
        // totalWidth/Stretch/Shrink after skipping leading glue from pos 0
        skipGlueAfter(-1, seed.totalWidth, seed.totalStretch, seed.totalShrink);
        seed.prevNode = -1;
        nodes.append(seed);
        active.append(0);
    }

    // Track whether the previous item was a box (needed for glue-as-breakpoint rule)
    bool prevWasBox = false;

    for (int i = 0; i < n; ++i) {
        const auto &item = items[i];

        // Determine if this position is a legal breakpoint
        bool isBreakpoint = false;
        if (item.type == Item::PenaltyType && item.penalty.penalty < 10000)
            isBreakpoint = true;
        if (item.type == Item::GlueType && prevWasBox)
            isBreakpoint = true;

        // Update prevWasBox for next iteration
        if (item.type == Item::BoxType)
            prevWasBox = true;
        else if (item.type != Item::PenaltyType) // glue resets it
            prevWasBox = false;
        // penalties don't change prevWasBox

        if (!isBreakpoint)
            continue;

        // The break position: for glue, we break *before* the glue (line ends
        // before this glue); for penalty, we break *at* the penalty.
        // In prefix-sum terms, the line content goes from the active node's
        // position up to index i.

        // Best candidate per fitness class
        struct Candidate {
            qreal demerits = INF;
            int nodeIdx = -1;
            FitnessClass fitness = Normal;
        };
        Candidate best[4]; // one per FitnessClass

        QList<int> toDeactivate;

        for (int ai = 0; ai < active.size(); ++ai) {
            int nodeIdx = active[ai];
            const Node &nd = nodes[nodeIdx];

            int lineNum = nd.lineNumber;
            qreal lw = lineWidthForLine(lineWidths, lineNum);

            // Line width from this node to current break
            qreal lineContentWidth = prefixWidth[i] - nd.totalWidth;
            qreal lineStretchTotal = prefixStretch[i] - nd.totalStretch;
            qreal lineShrinkTotal = prefixShrink[i] - nd.totalShrink;

            // If breaking at a penalty, add its width (hyphen)
            if (item.type == Item::PenaltyType)
                lineContentWidth += item.penalty.width;

            qreal shortfall = lw - lineContentWidth;
            qreal r;

            if (std::abs(shortfall) < 1e-10) {
                r = 0;
            } else if (shortfall > 0) {
                r = (lineStretchTotal > 1e-10) ? shortfall / lineStretchTotal : INF;
            } else {
                r = (lineShrinkTotal > 1e-10) ? shortfall / lineShrinkTotal : -INF;
            }

            // If r < -1, the line is overfull — deactivate this node
            if (r < -1.0) {
                toDeactivate.append(ai);
                continue;
            }

            // Check tolerance
            if (std::abs(r) > config.tolerance && r != -INF && r != INF) {
                // ratio too large — skip but don't deactivate
                // (a future break might still work from this node)
                continue;
            }

            // If r is infinity, skip (can't form a line)
            if (r == INF || r == -INF)
                continue;

            // Compute demerits
            qreal penalty = (item.type == Item::PenaltyType) ? item.penalty.penalty : 0;
            qreal badness = 100.0 * std::pow(std::abs(r), 3.0);
            qreal d;

            if (penalty >= 0) {
                d = std::pow(1.0 + badness + penalty, 2.0);
            } else if (penalty > -10000) {
                d = std::pow(1.0 + badness, 2.0) - penalty * penalty;
            } else {
                d = std::pow(1.0 + badness, 2.0);
            }

            // Consecutive hyphen penalty
            bool currentFlagged = (item.type == Item::PenaltyType && item.penalty.flagged);
            if (currentFlagged && nd.flagged)
                d += config.consecutiveHyphenDemerits;

            // Fitness class
            FitnessClass fc = fitnessClassFromRatio(r);

            // Fitness demerits if class differs by more than 1
            int fitDiff = std::abs(static_cast<int>(fc) - static_cast<int>(nd.fitness));
            if (fitDiff > 1)
                d += config.fitnessDemerits;

            qreal totalD = nd.totalDemerits + d;

            if (totalD < best[fc].demerits) {
                best[fc].demerits = totalD;
                best[fc].nodeIdx = nodeIdx;
                best[fc].fitness = fc;
            }
        }

        // Deactivate overfull nodes (in reverse to preserve indices)
        std::sort(toDeactivate.begin(), toDeactivate.end(), std::greater<int>());
        for (int ai : toDeactivate)
            active.removeAt(ai);

        // Emergency break: if no active nodes remain and no candidates found,
        // force a break from the most recent node with high demerits
        bool hasCandidates = false;
        for (int fc = 0; fc < 4; ++fc) {
            if (best[fc].nodeIdx >= 0) {
                hasCandidates = true;
                break;
            }
        }

        if (active.isEmpty() && !hasCandidates) {
            // Find the most recently created node to use as emergency predecessor
            // (it was just deactivated because of overfull)
            int emergencyPrev = -1;
            qreal bestDem = INF;
            for (int ni = nodes.size() - 1; ni >= 0; --ni) {
                if (nodes[ni].totalDemerits < bestDem) {
                    bestDem = nodes[ni].totalDemerits;
                    emergencyPrev = ni;
                }
            }

            if (emergencyPrev >= 0) {
                Node emergencyNode;
                emergencyNode.itemIndex = i + 1; // break after this item
                emergencyNode.lineNumber = nodes[emergencyPrev].lineNumber + 1;
                emergencyNode.fitness = Normal;
                emergencyNode.totalDemerits = bestDem + 1e10; // very high demerits
                emergencyNode.prevNode = emergencyPrev;
                emergencyNode.flagged = (item.type == Item::PenaltyType && item.penalty.flagged);
                skipGlueAfter(i, emergencyNode.totalWidth, emergencyNode.totalStretch, emergencyNode.totalShrink);
                int newIdx = nodes.size();
                nodes.append(emergencyNode);
                active.append(newIdx);
                result.optimal = false;
            }
            continue;
        }

        // Create new active nodes from the best candidates per fitness class
        for (int fc = 0; fc < 4; ++fc) {
            if (best[fc].nodeIdx < 0)
                continue;

            const Node &prev = nodes[best[fc].nodeIdx];
            Node newNode;
            newNode.itemIndex = i + 1; // the break is at item i; next line starts after
            newNode.lineNumber = prev.lineNumber + 1;
            newNode.fitness = best[fc].fitness;
            newNode.totalDemerits = best[fc].demerits;
            newNode.prevNode = best[fc].nodeIdx;
            newNode.flagged = (item.type == Item::PenaltyType && item.penalty.flagged);

            // Set prefix sums to values after skipping leading glue past break
            skipGlueAfter(i, newNode.totalWidth, newNode.totalStretch, newNode.totalShrink);

            nodes.append(newNode);
            active.append(nodes.size() - 1);
        }
    }

    // Find the active node with the lowest total demerits
    if (active.isEmpty()) {
        result.optimal = false;
        return result;
    }

    int bestIdx = active[0];
    qreal bestDem = nodes[active[0]].totalDemerits;
    for (int i = 1; i < active.size(); ++i) {
        if (nodes[active[i]].totalDemerits < bestDem) {
            bestDem = nodes[active[i]].totalDemerits;
            bestIdx = active[i];
        }
    }

    // Trace back through prevNode chain to collect breakpoints
    QList<int> breakNodeIndices;
    for (int idx = bestIdx; idx >= 0; idx = nodes[idx].prevNode)
        breakNodeIndices.prepend(idx);

    // Build the breakpoint list (skip the seed node at index 0 since it
    // represents the start-of-paragraph, not an actual line break)
    for (int k = 1; k < breakNodeIndices.size(); ++k) {
        const Node &nd = nodes[breakNodeIndices[k]];
        const Node &prev = nodes[breakNodeIndices[k - 1]];

        // Recompute adjustment ratio for this line
        int lineStart = prev.itemIndex;
        int lineEnd = nd.itemIndex;
        qreal lw = lineWidthForLine(lineWidths, prev.lineNumber);

        // Use prefix sums for the recompute, but fall back to the direct
        // function for correctness
        qreal r = computeAdjustmentRatio(items, lineStart, lineEnd, lw);

        Breakpoint bp;
        bp.itemIndex = nd.itemIndex;
        bp.adjustmentRatio = r;
        bp.fitness = fitnessClassFromRatio(r);
        bp.totalDemerits = nd.totalDemerits;
        result.breaks.append(bp);
    }

    return result;
}

// ---------------------------------------------------------------------------
// findBreaksTiered
// ---------------------------------------------------------------------------
BreakResult findBreaksTiered(const QList<Item> &items,
                             const QList<qreal> &lineWidths,
                             const Config &baseConfig)
{
    if (items.isEmpty() || lineWidths.isEmpty()) {
        BreakResult result;
        result.optimal = false;
        return result;
    }

    // Tier 1: strict tolerance
    {
        Config cfg = baseConfig;
        cfg.tolerance = 1.0;
        auto result = findBreaks(items, lineWidths, cfg);
        if (result.optimal && !result.breaks.isEmpty())
            return result;
    }

    // Tier 2: relaxed tolerance
    {
        Config cfg = baseConfig;
        cfg.tolerance = 2.0;
        auto result = findBreaks(items, lineWidths, cfg);
        if (result.optimal && !result.breaks.isEmpty())
            return result;
    }

    // Tier 3: emergency tolerance
    {
        Config cfg = baseConfig;
        cfg.tolerance = baseConfig.looseTolerance;
        auto result = findBreaks(items, lineWidths, cfg);
        if (!result.breaks.isEmpty())
            return result;
    }

    // Tier 4: give up — return non-optimal empty result to trigger greedy fallback
    BreakResult result;
    result.optimal = false;
    return result;
}

// ---------------------------------------------------------------------------
// computeBlendedSpacing
// ---------------------------------------------------------------------------
BlendedSpacing computeBlendedSpacing(qreal adjustmentRatio,
                                      qreal naturalWordGlueWidth,
                                      int wordGapCount,
                                      int charCount,
                                      qreal fontSize,
                                      const Config &config)
{
    BlendedSpacing spacing;

    if (wordGapCount <= 0 || std::abs(adjustmentRatio) < 1e-10)
        return spacing;

    // Compute total slack based on how much the word glue would flex
    qreal totalSlack = 0;
    if (adjustmentRatio > 0) {
        // Stretching: glue stretch is typically 0.5 * natural width
        totalSlack = adjustmentRatio * (naturalWordGlueWidth * 0.5) * wordGapCount;
    } else {
        // Shrinking: glue shrink is typically 0.33 * natural width
        totalSlack = adjustmentRatio * (naturalWordGlueWidth * 0.33) * wordGapCount;
    }

    // Split: 2/3 to word spacing, 1/3 to letter spacing
    qreal wordSlack = totalSlack * (2.0 / 3.0);
    qreal letterSlack = totalSlack * (1.0 / 3.0);

    // Letter spacing per character
    qreal letterSpacingPerChar = 0;
    if (charCount > 0)
        letterSpacingPerChar = letterSlack / charCount;

    // Clamp letter spacing
    qreal minLS = config.minLetterSpacingFraction * fontSize;
    qreal maxLS = config.maxLetterSpacingFraction * fontSize;
    qreal clampedLS = qBound(minLS, letterSpacingPerChar, maxLS);

    // Compute actual letter slack after clamping
    qreal actualLetterSlack = clampedLS * charCount;

    // Remainder goes to word spacing
    qreal remainderSlack = letterSlack - actualLetterSlack;
    qreal totalWordSlack = wordSlack + remainderSlack;

    spacing.extraWordSpacing = totalWordSlack / wordGapCount;
    spacing.extraLetterSpacing = clampedLS;

    return spacing;
}

} // namespace LineBreaking
