/*
 * fontpairingmanager.cpp — Discovery/loading/saving for font pairings
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "fontpairingmanager.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStandardPaths>

FontPairingManager::FontPairingManager(QObject *parent)
    : QObject(parent)
{
    discoverPairings();
}

// ---------------------------------------------------------------------------
// Discovery
// ---------------------------------------------------------------------------

static QString userPairingsDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
           + QLatin1String("/pairings");
}

void FontPairingManager::discoverPairings()
{
    // Built-in pairings bundled as Qt resources
    QDir resourceDir(QStringLiteral(":/pairings"));
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
        if (root.value(QLatin1String("type")).toString() != QLatin1String("fontPairing"))
            continue;

        QString id = root.value(QLatin1String("id")).toString();
        if (id.isEmpty())
            id = QFileInfo(entry).completeBaseName();
        QString name = root.value(QLatin1String("name")).toString(id);

        m_pairings.append({id, name, path, true});
    }

    // User pairings from XDG data directory
    QDir dir(userPairingsDir());
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
            if (root.value(QLatin1String("type")).toString() != QLatin1String("fontPairing"))
                continue;

            QString id = root.value(QLatin1String("id")).toString();
            if (id.isEmpty())
                id = QFileInfo(entry).completeBaseName();

            // Skip if a built-in already owns this ID
            bool alreadyKnown = false;
            for (const auto &existing : m_pairings) {
                if (existing.id == id) { alreadyKnown = true; break; }
            }
            if (alreadyKnown)
                continue;

            QString name = root.value(QLatin1String("name")).toString(id);
            m_pairings.append({id, name, path, false});
        }
    }
}

// ---------------------------------------------------------------------------
// Accessors
// ---------------------------------------------------------------------------

QStringList FontPairingManager::availablePairings() const
{
    QStringList ids;
    for (const auto &p : m_pairings)
        ids.append(p.id);
    return ids;
}

QString FontPairingManager::pairingName(const QString &id) const
{
    for (const auto &p : m_pairings) {
        if (p.id == id)
            return p.name;
    }
    return id;
}

FontPairing FontPairingManager::pairing(const QString &id) const
{
    for (const auto &p : m_pairings) {
        if (p.id == id) {
            QFile file(p.path);
            if (!file.open(QIODevice::ReadOnly))
                return {};

            QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
            if (doc.isNull())
                return {};

            return FontPairing::fromJson(doc.object());
        }
    }
    return {};
}

bool FontPairingManager::isBuiltin(const QString &id) const
{
    for (const auto &p : m_pairings) {
        if (p.id == id)
            return p.builtin;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Save / Delete
// ---------------------------------------------------------------------------

QString FontPairingManager::savePairing(const FontPairing &pairing)
{
    QString dir = userPairingsDir();
    QDir().mkpath(dir);

    QString id = pairing.id;
    if (id.isEmpty()) {
        QString base = pairing.name.toLower().replace(
            QRegularExpression(QStringLiteral("[^a-z0-9]+")),
            QStringLiteral("-"));
        if (base.isEmpty())
            base = QStringLiteral("pairing");
        id = base;
        // Ensure filename uniqueness
        QString path = dir + QLatin1Char('/') + id + QLatin1String(".json");
        int suffix = 1;
        while (QFile::exists(path)) {
            id = base + QLatin1Char('-') + QString::number(suffix++);
            path = dir + QLatin1Char('/') + id + QLatin1String(".json");
        }
    }

    // Check if we are overwriting an existing user pairing
    bool found = false;
    for (auto &p : m_pairings) {
        if (p.id == id) {
            if (p.builtin)
                return {}; // cannot overwrite built-in
            found = true;
            break;
        }
    }

    QString path = dir + QLatin1Char('/') + id + QLatin1String(".json");

    // Build JSON via the value class serialization
    FontPairing toSave = pairing;
    toSave.id = id;
    QJsonDocument doc(toSave.toJson());

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return {};

    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

    if (!found) {
        m_pairings.append({id, toSave.name, path, false});
    } else {
        for (auto &p : m_pairings) {
            if (p.id == id) {
                p.name = toSave.name;
                p.path = path;
                break;
            }
        }
    }

    Q_EMIT pairingsChanged();
    return id;
}

bool FontPairingManager::deletePairing(const QString &id)
{
    for (int i = 0; i < m_pairings.size(); ++i) {
        if (m_pairings[i].id == id) {
            if (m_pairings[i].builtin)
                return false; // cannot delete built-in

            if (!QFile::remove(m_pairings[i].path))
                qWarning("FontPairingManager: failed to remove %s",
                         qPrintable(m_pairings[i].path));
            m_pairings.removeAt(i);
            Q_EMIT pairingsChanged();
            return true;
        }
    }
    return false;
}
