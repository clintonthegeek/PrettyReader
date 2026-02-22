/*
 * fontdegradationmap.h — TTF/OTF to Hershey font family mapping
 *
 * Provides a static lookup table that maps common TTF/OTF font family
 * names to their closest Hershey vector-font counterpart.  Used as a
 * fallback when the TypeSet does not contain an explicit mapping
 * for a given family.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef PRETTYREADER_FONTDEGRADATIONMAP_H
#define PRETTYREADER_FONTDEGRADATIONMAP_H

#include <QString>

class FontDegradationMap
{
public:
    /// Returns the best Hershey family name for the given TTF/OTF family.
    ///
    /// Uses a static table of known mappings first, then falls back to
    /// generic classification heuristics.  If nothing matches, returns
    /// "Hershey Sans" as the most neutral default.
    static QString hersheyFamilyFor(const QString &fontFamily);

private:
    FontDegradationMap() = delete;
};

#endif // PRETTYREADER_FONTDEGRADATIONMAP_H
