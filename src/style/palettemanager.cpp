/*
 * palettemanager.cpp — Discovery/loading/saving for color palettes
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "palettemanager.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStandardPaths>

PaletteManager::PaletteManager(QObject *parent)
    : QObject(parent)
{
    discoverPalettes();
}

// ---------------------------------------------------------------------------
// Discovery
// ---------------------------------------------------------------------------

static QString userPalettesDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
           + QLatin1String("/palettes");
}

void PaletteManager::discoverPalettes()
{
    // Built-in palettes bundled as Qt resources
    QDir resourceDir(QStringLiteral(":/palettes"));
    const QStringList entries = resourceDir.entryList(
        {QStringLiteral("*.json")}, QDir::Files);

    for (const QString &entry : entries) {
        QString path = resourceDir.filePath(entry);
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly))
            continue;

        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        if (doc.isNull())
            continue;

        QJsonObject root = doc.object();
        if (root.value(QLatin1String("type")).toString() != QLatin1String("colorPalette"))
            continue;

        QString id = root.value(QLatin1String("id")).toString();
        if (id.isEmpty())
            id = QFileInfo(entry).completeBaseName();
        QString name = root.value(QLatin1String("name")).toString(id);

        m_palettes.append({id, name, path, true});
    }

    // User palettes from XDG data directory
    QDir dir(userPalettesDir());
    if (dir.exists()) {
        const QStringList userEntries = dir.entryList(
            {QStringLiteral("*.json")}, QDir::Files);
        for (const QString &entry : userEntries) {
            QString path = dir.filePath(entry);
            QFile file(path);
            if (!file.open(QIODevice::ReadOnly))
                continue;

            QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
            if (doc.isNull())
                continue;

            QJsonObject root = doc.object();
            if (root.value(QLatin1String("type")).toString() != QLatin1String("colorPalette"))
                continue;

            QString id = root.value(QLatin1String("id")).toString();
            if (id.isEmpty())
                id = QFileInfo(entry).completeBaseName();

            // Skip if a built-in already owns this ID
            bool alreadyKnown = false;
            for (const auto &existing : m_palettes) {
                if (existing.id == id) { alreadyKnown = true; break; }
            }
            if (alreadyKnown)
                continue;

            QString name = root.value(QLatin1String("name")).toString(id);
            m_palettes.append({id, name, path, false});
        }
    }
}

// ---------------------------------------------------------------------------
// Accessors
// ---------------------------------------------------------------------------

QStringList PaletteManager::availablePalettes() const
{
    QStringList ids;
    for (const auto &p : m_palettes)
        ids.append(p.id);
    return ids;
}

QString PaletteManager::paletteName(const QString &id) const
{
    for (const auto &p : m_palettes) {
        if (p.id == id)
            return p.name;
    }
    return id;
}

ColorPalette PaletteManager::palette(const QString &id) const
{
    for (const auto &p : m_palettes) {
        if (p.id == id) {
            QFile file(p.path);
            if (!file.open(QIODevice::ReadOnly))
                return {};

            QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
            if (doc.isNull())
                return {};

            return ColorPalette::fromJson(doc.object());
        }
    }
    return {};
}

bool PaletteManager::isBuiltin(const QString &id) const
{
    for (const auto &p : m_palettes) {
        if (p.id == id)
            return p.builtin;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Save / Delete
// ---------------------------------------------------------------------------

QString PaletteManager::savePalette(const ColorPalette &palette)
{
    QString dir = userPalettesDir();
    QDir().mkpath(dir);

    QString id = palette.id;
    if (id.isEmpty()) {
        QString base = palette.name.toLower().replace(
            QRegularExpression(QStringLiteral("[^a-z0-9]+")),
            QStringLiteral("-"));
        if (base.isEmpty())
            base = QStringLiteral("palette");
        id = base;
        // Ensure filename uniqueness
        QString path = dir + QLatin1Char('/') + id + QLatin1String(".json");
        int suffix = 1;
        while (QFile::exists(path)) {
            id = base + QLatin1Char('-') + QString::number(suffix++);
            path = dir + QLatin1Char('/') + id + QLatin1String(".json");
        }
    }

    // Check if we are overwriting an existing user palette
    bool found = false;
    for (auto &p : m_palettes) {
        if (p.id == id) {
            if (p.builtin)
                return {}; // cannot overwrite built-in
            found = true;
            break;
        }
    }

    QString path = dir + QLatin1Char('/') + id + QLatin1String(".json");

    // Build JSON via the value class serialization
    ColorPalette toSave = palette;
    toSave.id = id;
    QJsonDocument doc(toSave.toJson());

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return {};

    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

    if (!found) {
        m_palettes.append({id, toSave.name, path, false});
    } else {
        for (auto &p : m_palettes) {
            if (p.id == id) {
                p.name = toSave.name;
                p.path = path;
                break;
            }
        }
    }

    Q_EMIT palettesChanged();
    return id;
}

bool PaletteManager::deletePalette(const QString &id)
{
    for (int i = 0; i < m_palettes.size(); ++i) {
        if (m_palettes[i].id == id) {
            if (m_palettes[i].builtin)
                return false; // cannot delete built-in

            if (!QFile::remove(m_palettes[i].path))
                qWarning("PaletteManager: failed to remove %s",
                         qPrintable(m_palettes[i].path));
            m_palettes.removeAt(i);
            Q_EMIT palettesChanged();
            return true;
        }
    }
    return false;
}
