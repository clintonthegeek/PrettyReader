/*
 * fontpairing.h — Typographic role triplet for theme composition
 *
 * Bundles three font roles (body, heading, mono) each with a
 * TTF/OTF family and a Hershey vector-font fallback.  Used as one
 * of the two independent axes of the theme system (the other being
 * ColorPalette).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef PRETTYREADER_FONTPAIRING_H
#define PRETTYREADER_FONTPAIRING_H

#include <QJsonObject>
#include <QString>

struct FontRole {
    QString family;        // e.g. "Noto Serif"
    QString hersheyFamily; // e.g. "Hershey Serif"

    bool operator==(const FontRole &other) const {
        return family == other.family && hersheyFamily == other.hersheyFamily;
    }
};

class FontPairing
{
public:
    FontPairing() = default;

    QString id;          // kebab-case, e.g. "noto-serif-sans-jetbrains"
    QString name;        // display name
    QString description;

    FontRole body;
    FontRole heading;
    FontRole mono;

    /// Look up the Hershey fallback family for a given TTF/OTF family.
    ///
    /// If @p ttfFamily matches one of the three roles' family names the
    /// corresponding hersheyFamily is returned directly.  Otherwise the
    /// request is forwarded to FontDegradationMap::hersheyFamilyFor().
    QString hersheyFamilyFor(const QString &ttfFamily) const;

    // --- Serialization ---

    bool operator==(const FontPairing &other) const {
        return id == other.id && body == other.body
               && heading == other.heading && mono == other.mono;
    }

    static FontPairing fromJson(const QJsonObject &obj);
    QJsonObject toJson() const;
};

#endif // PRETTYREADER_FONTPAIRING_H
