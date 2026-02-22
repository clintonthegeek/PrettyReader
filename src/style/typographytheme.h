/*
 * typographytheme.h — Typography axis of the two-axis theme system
 *
 * Bundles font roles (body, heading, mono), sizing, spacing, and
 * all non-color style overrides.  Combined with a ColorPalette by
 * ThemeComposer to produce the final styled document.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef PRETTYREADER_TYPOGRAPHYTHEME_H
#define PRETTYREADER_TYPOGRAPHYTHEME_H

#include <QJsonObject>
#include <QString>

struct FontRole {
    QString family;        // e.g. "Noto Serif"
    QString hersheyFamily; // e.g. "Hershey Serif"

    bool operator==(const FontRole &other) const {
        return family == other.family && hersheyFamily == other.hersheyFamily;
    }
};

class TypographyTheme
{
public:
    TypographyTheme() = default;

    QString id;          // kebab-case, e.g. "default"
    QString name;        // display name
    QString description;
    int version = 1;
    bool hersheyMode = false;

    FontRole body;
    FontRole heading;
    FontRole mono;

    // Style overrides as raw JSON — applied by ThemeManager::applyStyleOverrides
    QJsonObject paragraphStyles;
    QJsonObject characterStyles;
    QJsonObject tableStyles;
    QJsonObject footnoteStyle;
    QJsonObject masterPages;
    QJsonObject pageLayout;

    /// Look up the Hershey fallback family for a given TTF/OTF family.
    QString hersheyFamilyFor(const QString &ttfFamily) const;

    bool operator==(const TypographyTheme &other) const {
        return id == other.id && body == other.body
               && heading == other.heading && mono == other.mono;
    }

    static TypographyTheme fromJson(const QJsonObject &obj);
    QJsonObject toJson() const;
};

#endif // PRETTYREADER_TYPOGRAPHYTHEME_H
