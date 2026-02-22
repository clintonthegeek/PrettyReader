/*
 * typeset.cpp — Typography axis of the three-axis theme system
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "typeset.h"
#include "fontdegradationmap.h"

#include <QJsonArray>
#include <QJsonObject>

// ---------------------------------------------------------------------------
// hersheyFamilyFor
// ---------------------------------------------------------------------------

QString TypeSet::hersheyFamilyFor(const QString &ttfFamily) const
{
    if (ttfFamily.compare(body.family, Qt::CaseInsensitive) == 0)
        return body.hersheyFamily;
    if (ttfFamily.compare(heading.family, Qt::CaseInsensitive) == 0)
        return heading.hersheyFamily;
    if (ttfFamily.compare(mono.family, Qt::CaseInsensitive) == 0)
        return mono.hersheyFamily;

    return FontDegradationMap::hersheyFamilyFor(ttfFamily);
}

// ---------------------------------------------------------------------------
// JSON helpers
// ---------------------------------------------------------------------------

static FontRole fontRoleFromJson(const QJsonObject &obj)
{
    FontRole role;
    role.family        = obj.value(QLatin1String("family")).toString();
    role.hersheyFamily = obj.value(QLatin1String("hersheyFamily")).toString();
    return role;
}

static QJsonObject fontRoleToJson(const FontRole &role)
{
    QJsonObject obj;
    obj[QLatin1String("family")]        = role.family;
    obj[QLatin1String("hersheyFamily")] = role.hersheyFamily;
    return obj;
}

// Strip any color keys from a JSON object (defense-in-depth)
static void stripColorKeys(QJsonObject &obj)
{
    static const QStringList colorKeys = {
        QStringLiteral("foreground"),
        QStringLiteral("background"),
        QStringLiteral("color"),
    };

    for (const QString &key : colorKeys)
        obj.remove(key);
}

static QJsonObject stripColorsFromStyleBlock(const QJsonObject &block)
{
    QJsonObject result;
    for (auto it = block.begin(); it != block.end(); ++it) {
        QJsonObject style = it.value().toObject();
        stripColorKeys(style);

        // Also strip color from nested border objects
        for (const QString &borderKey : {QStringLiteral("outerBorder"),
                                          QStringLiteral("innerBorder"),
                                          QStringLiteral("headerBottomBorder")}) {
            if (style.contains(borderKey)) {
                QJsonObject border = style.value(borderKey).toObject();
                stripColorKeys(border);
                style[borderKey] = border;
            }
        }

        // Strip table color keys
        static const QStringList tableColorKeys = {
            QStringLiteral("headerBackground"),
            QStringLiteral("headerForeground"),
            QStringLiteral("bodyBackground"),
            QStringLiteral("alternateRowColor"),
        };
        for (const QString &k : tableColorKeys)
            style.remove(k);

        result[it.key()] = style;
    }
    return result;
}

// ---------------------------------------------------------------------------
// fromJson / toJson
// ---------------------------------------------------------------------------

TypeSet TypeSet::fromJson(const QJsonObject &obj)
{
    TypeSet ts;

    ts.id          = obj.value(QLatin1String("id")).toString();
    ts.name        = obj.value(QLatin1String("name")).toString();
    ts.description = obj.value(QLatin1String("description")).toString();
    ts.version     = obj.value(QLatin1String("version")).toInt(1);
    ts.hersheyMode = obj.value(QLatin1String("hersheyMode")).toBool(false);

    // Font roles — stored under "fonts"
    QJsonObject fonts = obj.value(QLatin1String("fonts")).toObject();
    ts.body    = fontRoleFromJson(fonts.value(QLatin1String("body")).toObject());
    ts.heading = fontRoleFromJson(fonts.value(QLatin1String("heading")).toObject());
    ts.mono    = fontRoleFromJson(fonts.value(QLatin1String("mono")).toObject());

    // Style override blocks
    ts.paragraphStyles = obj.value(QLatin1String("paragraphStyles")).toObject();
    ts.characterStyles = obj.value(QLatin1String("characterStyles")).toObject();
    ts.tableStyles     = obj.value(QLatin1String("tableStyles")).toObject();
    ts.footnoteStyle   = obj.value(QLatin1String("footnoteStyle")).toObject();

    return ts;
}

QJsonObject TypeSet::toJson() const
{
    QJsonObject obj;

    if (!id.isEmpty())
        obj[QLatin1String("id")] = id;
    obj[QLatin1String("name")]    = name;
    obj[QLatin1String("version")] = version;
    obj[QLatin1String("type")]    = QStringLiteral("typeSet");

    if (!description.isEmpty())
        obj[QLatin1String("description")] = description;

    if (hersheyMode)
        obj[QLatin1String("hersheyMode")] = true;

    QJsonObject fonts;
    fonts[QLatin1String("body")]    = fontRoleToJson(body);
    fonts[QLatin1String("heading")] = fontRoleToJson(heading);
    fonts[QLatin1String("mono")]    = fontRoleToJson(mono);
    obj[QLatin1String("fonts")] = fonts;

    // Strip any color keys before writing (defense-in-depth)
    if (!paragraphStyles.isEmpty())
        obj[QLatin1String("paragraphStyles")] = stripColorsFromStyleBlock(paragraphStyles);
    if (!characterStyles.isEmpty())
        obj[QLatin1String("characterStyles")] = stripColorsFromStyleBlock(characterStyles);
    if (!tableStyles.isEmpty())
        obj[QLatin1String("tableStyles")] = stripColorsFromStyleBlock(tableStyles);
    if (!footnoteStyle.isEmpty())
        obj[QLatin1String("footnoteStyle")] = footnoteStyle;

    return obj;
}
