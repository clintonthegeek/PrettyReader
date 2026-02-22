/*
 * fontpairing.cpp — Typographic role triplet for theme composition
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "fontpairing.h"
#include "fontdegradationmap.h"

#include <QJsonObject>

// ---------------------------------------------------------------------------
// hersheyFamilyFor
// ---------------------------------------------------------------------------

QString FontPairing::hersheyFamilyFor(const QString &ttfFamily) const
{
    // Check our three explicit role mappings first (case-insensitive)
    if (ttfFamily.compare(body.family, Qt::CaseInsensitive) == 0)
        return body.hersheyFamily;
    if (ttfFamily.compare(heading.family, Qt::CaseInsensitive) == 0)
        return heading.hersheyFamily;
    if (ttfFamily.compare(mono.family, Qt::CaseInsensitive) == 0)
        return mono.hersheyFamily;

    // Fall back to the global degradation map
    return FontDegradationMap::hersheyFamilyFor(ttfFamily);
}

// ---------------------------------------------------------------------------
// JSON serialization
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

FontPairing FontPairing::fromJson(const QJsonObject &obj)
{
    FontPairing pairing;

    pairing.id          = obj.value(QLatin1String("id")).toString();
    pairing.name        = obj.value(QLatin1String("name")).toString();
    pairing.description = obj.value(QLatin1String("description")).toString();

    QJsonObject roles = obj.value(QLatin1String("roles")).toObject();
    pairing.body    = fontRoleFromJson(roles.value(QLatin1String("body")).toObject());
    pairing.heading = fontRoleFromJson(roles.value(QLatin1String("heading")).toObject());
    pairing.mono    = fontRoleFromJson(roles.value(QLatin1String("mono")).toObject());

    return pairing;
}

QJsonObject FontPairing::toJson() const
{
    QJsonObject obj;

    if (!id.isEmpty())
        obj[QLatin1String("id")] = id;
    obj[QLatin1String("name")]    = name;
    obj[QLatin1String("version")] = 1;
    obj[QLatin1String("type")]    = QStringLiteral("fontPairing");

    if (!description.isEmpty())
        obj[QLatin1String("description")] = description;

    QJsonObject roles;
    roles[QLatin1String("body")]    = fontRoleToJson(body);
    roles[QLatin1String("heading")] = fontRoleToJson(heading);
    roles[QLatin1String("mono")]    = fontRoleToJson(mono);
    obj[QLatin1String("roles")] = roles;

    return obj;
}
