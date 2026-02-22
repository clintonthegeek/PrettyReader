/*
 * colorpalette.cpp — Semantic color palette for theme composition
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "colorpalette.h"

#include <QJsonObject>

// ---------------------------------------------------------------------------
// Convenience accessors
// ---------------------------------------------------------------------------

static QColor colorOrDefault(const QHash<QString, QColor> &colors,
                              const QString &role, const QColor &fallback)
{
    auto it = colors.constFind(role);
    return (it != colors.constEnd()) ? it.value() : fallback;
}

static const QColor s_defaultFg{0x00, 0x00, 0x00};   // black
static const QColor s_defaultBg{0xff, 0xff, 0xff};   // white

QColor ColorPalette::text() const
{
    return colorOrDefault(colors, QStringLiteral("text"), s_defaultFg);
}

QColor ColorPalette::headingText() const
{
    return colorOrDefault(colors, QStringLiteral("headingText"), s_defaultFg);
}

QColor ColorPalette::blockquoteText() const
{
    return colorOrDefault(colors, QStringLiteral("blockquoteText"), s_defaultFg);
}

QColor ColorPalette::linkText() const
{
    return colorOrDefault(colors, QStringLiteral("linkText"), s_defaultFg);
}

QColor ColorPalette::codeText() const
{
    return colorOrDefault(colors, QStringLiteral("codeText"), s_defaultFg);
}

QColor ColorPalette::surfaceCode() const
{
    return colorOrDefault(colors, QStringLiteral("surfaceCode"), s_defaultBg);
}

QColor ColorPalette::surfaceInlineCode() const
{
    return colorOrDefault(colors, QStringLiteral("surfaceInlineCode"), s_defaultBg);
}

QColor ColorPalette::surfaceTableHeader() const
{
    return colorOrDefault(colors, QStringLiteral("surfaceTableHeader"), s_defaultBg);
}

QColor ColorPalette::surfaceTableAlt() const
{
    return colorOrDefault(colors, QStringLiteral("surfaceTableAlt"), s_defaultBg);
}

QColor ColorPalette::pageBackground() const
{
    return colorOrDefault(colors, QStringLiteral("pageBackground"), s_defaultBg);
}

QColor ColorPalette::borderOuter() const
{
    return colorOrDefault(colors, QStringLiteral("borderOuter"), s_defaultFg);
}

QColor ColorPalette::borderInner() const
{
    return colorOrDefault(colors, QStringLiteral("borderInner"), s_defaultFg);
}

QColor ColorPalette::borderHeaderBottom() const
{
    return colorOrDefault(colors, QStringLiteral("borderHeaderBottom"), s_defaultFg);
}

// ---------------------------------------------------------------------------
// hasNonWhiteBackgrounds
// ---------------------------------------------------------------------------

static bool isNonWhiteAndOpaque(const QColor &c)
{
    return c.isValid() && c != Qt::white && c.alpha() == 255;
}

bool ColorPalette::hasNonWhiteBackgrounds() const
{
    // Check pageBackground
    if (isNonWhiteAndOpaque(pageBackground()))
        return true;

    // Check all surface* roles
    static const QString surfaceRoles[] = {
        QStringLiteral("surfaceCode"),
        QStringLiteral("surfaceInlineCode"),
        QStringLiteral("surfaceTableHeader"),
        QStringLiteral("surfaceTableAlt"),
    };
    for (const QString &role : surfaceRoles) {
        auto it = colors.constFind(role);
        if (it != colors.constEnd() && isNonWhiteAndOpaque(it.value()))
            return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// JSON serialization
// ---------------------------------------------------------------------------

ColorPalette ColorPalette::fromJson(const QJsonObject &obj)
{
    ColorPalette palette;

    palette.id          = obj.value(QLatin1String("id")).toString();
    palette.name        = obj.value(QLatin1String("name")).toString();
    palette.description = obj.value(QLatin1String("description")).toString();

    QJsonObject colorsObj = obj.value(QLatin1String("colors")).toObject();
    for (auto it = colorsObj.begin(); it != colorsObj.end(); ++it) {
        QColor c(it.value().toString());
        if (c.isValid())
            palette.colors.insert(it.key(), c);
    }

    return palette;
}

QJsonObject ColorPalette::toJson() const
{
    QJsonObject obj;

    if (!id.isEmpty())
        obj[QLatin1String("id")] = id;
    obj[QLatin1String("name")]    = name;
    obj[QLatin1String("version")] = 1;
    obj[QLatin1String("type")]    = QStringLiteral("colorPalette");

    if (!description.isEmpty())
        obj[QLatin1String("description")] = description;

    QJsonObject colorsObj;
    for (auto it = colors.constBegin(); it != colors.constEnd(); ++it)
        colorsObj[it.key()] = it.value().name();
    obj[QLatin1String("colors")] = colorsObj;

    return obj;
}
