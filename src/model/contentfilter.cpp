/*
 * contentfilter.cpp — Filter Content::Document by section selection
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "contentfilter.h"

namespace ContentFilter {

Content::Document filterSections(const Content::Document &doc,
                                  const QSet<int> &excludedHeadingIndices)
{
    if (excludedHeadingIndices.isEmpty())
        return doc;

    // Build a set of block indices to exclude.
    // For each excluded heading at index i with level L, exclude blocks
    // i..j-1 where j is the next heading with level <= L (or end of doc).
    QSet<int> excludedBlocks;

    for (int idx : excludedHeadingIndices) {
        if (idx < 0 || idx >= doc.blocks.size())
            continue;

        const auto *heading = std::get_if<Content::Heading>(&doc.blocks[idx]);
        if (!heading)
            continue;

        int level = heading->level;
        excludedBlocks.insert(idx);

        // Exclude subsequent blocks until we hit a heading of same or higher level
        for (int j = idx + 1; j < doc.blocks.size(); ++j) {
            const auto *nextHeading = std::get_if<Content::Heading>(&doc.blocks[j]);
            if (nextHeading && nextHeading->level <= level)
                break;
            excludedBlocks.insert(j);
        }
    }

    // Build filtered document
    Content::Document filtered;
    for (int i = 0; i < doc.blocks.size(); ++i) {
        if (!excludedBlocks.contains(i))
            filtered.blocks.append(doc.blocks[i]);
    }

    return filtered;
}

} // namespace ContentFilter
