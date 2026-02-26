/*
 * palettemanager.cpp — Discovery/loading/saving for color palettes
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "palettemanager.h"

#include <QStandardPaths>

static QString userPalettesDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
           + QLatin1String("/palettes");
}

PaletteManager::PaletteManager(QObject *parent)
    : QObject(parent)
{
    m_store.discover(
        QStringLiteral("palettes"),
        [](const QJsonObject &root) {
            return root.value(QLatin1String("type")).toString()
                   == QLatin1String("colorPalette");
        },
        {userPalettesDir()});
}

ColorPalette PaletteManager::palette(const QString &id) const
{
    QJsonObject json = m_store.loadJson(id);
    return json.isEmpty() ? ColorPalette{} : ColorPalette::fromJson(json);
}

QString PaletteManager::savePalette(const ColorPalette &palette)
{
    ColorPalette toSave = palette;
    if (toSave.id.isEmpty())
        toSave.id = QStringLiteral("placeholder");
    QString id = m_store.save(palette.id, palette.name,
                              toSave.toJson(), QStringLiteral("palette"));
    if (!id.isEmpty())
        Q_EMIT palettesChanged();
    return id;
}

bool PaletteManager::deletePalette(const QString &id)
{
    if (m_store.remove(id, "PaletteManager")) {
        Q_EMIT palettesChanged();
        return true;
    }
    return false;
}
