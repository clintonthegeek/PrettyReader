/*
 * pagetemplate.cpp — Page layout template serialization
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "pagetemplate.h"

PageTemplate PageTemplate::fromJson(const QJsonObject &obj)
{
    PageTemplate tmpl;

    tmpl.id          = obj.value(QLatin1String("id")).toString();
    tmpl.name        = obj.value(QLatin1String("name")).toString();
    tmpl.description = obj.value(QLatin1String("description")).toString();
    tmpl.version     = obj.value(QLatin1String("version")).toInt(1);

    QJsonObject plObj = obj.value(QLatin1String("pageLayout")).toObject();
    QJsonObject mpObj = obj.value(QLatin1String("masterPages")).toObject();
    tmpl.pageLayout = PageLayout::fromJson(plObj, mpObj);

    return tmpl;
}

QJsonObject PageTemplate::toJson() const
{
    QJsonObject obj;

    if (!id.isEmpty())
        obj[QLatin1String("id")] = id;
    obj[QLatin1String("name")]    = name;
    obj[QLatin1String("version")] = version;
    obj[QLatin1String("type")]    = QStringLiteral("pageTemplate");

    if (!description.isEmpty())
        obj[QLatin1String("description")] = description;

    obj[QLatin1String("pageLayout")] = pageLayout.toPageLayoutJson();

    QJsonObject mp = pageLayout.toMasterPagesJson();
    if (!mp.isEmpty())
        obj[QLatin1String("masterPages")] = mp;

    return obj;
}
