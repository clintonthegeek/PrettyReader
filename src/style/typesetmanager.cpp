/*
 * typesetmanager.cpp — Discovery/loading/saving for type sets
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "typesetmanager.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStandardPaths>

TypeSetManager::TypeSetManager(QObject *parent)
    : QObject(parent)
{
    discoverTypeSets();
}

// ---------------------------------------------------------------------------
// Discovery
// ---------------------------------------------------------------------------

static QString userTypeSetsDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
           + QLatin1String("/typesets");
}

static QString legacyUserTypeSetsDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
           + QLatin1String("/typography");
}

static bool isTypeSetJson(const QJsonObject &root)
{
    QString type = root.value(QLatin1String("type")).toString();
    return type == QLatin1String("typeSet") || type == QLatin1String("typographyTheme");
}

void TypeSetManager::discoverTypeSets()
{
    // Built-in type sets bundled as Qt resources
    QDir resourceDir(QStringLiteral(":/typesets"));
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
        if (!isTypeSetJson(root))
            continue;

        QString id = root.value(QLatin1String("id")).toString();
        if (id.isEmpty())
            id = QFileInfo(entry).completeBaseName();
        QString name = root.value(QLatin1String("name")).toString(id);

        m_typeSets.append({id, name, path, true});
    }

    // User type sets from XDG data directory (new path + legacy fallback)
    auto scanUserDir = [this](const QString &dirPath) {
        QDir dir(dirPath);
        if (!dir.exists())
            return;
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
            if (!isTypeSetJson(root))
                continue;

            QString id = root.value(QLatin1String("id")).toString();
            if (id.isEmpty())
                id = QFileInfo(entry).completeBaseName();

            // Skip if already known
            bool alreadyKnown = false;
            for (const auto &existing : m_typeSets) {
                if (existing.id == id) { alreadyKnown = true; break; }
            }
            if (alreadyKnown)
                continue;

            QString name = root.value(QLatin1String("name")).toString(id);
            m_typeSets.append({id, name, path, false});
        }
    };

    scanUserDir(userTypeSetsDir());
    scanUserDir(legacyUserTypeSetsDir());
}

// ---------------------------------------------------------------------------
// Accessors
// ---------------------------------------------------------------------------

QStringList TypeSetManager::availableTypeSets() const
{
    QStringList ids;
    for (const auto &t : m_typeSets)
        ids.append(t.id);
    return ids;
}

QString TypeSetManager::typeSetName(const QString &id) const
{
    for (const auto &t : m_typeSets) {
        if (t.id == id)
            return t.name;
    }
    return id;
}

TypeSet TypeSetManager::typeSet(const QString &id) const
{
    for (const auto &t : m_typeSets) {
        if (t.id == id) {
            QFile file(t.path);
            if (!file.open(QIODevice::ReadOnly))
                return {};

            QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
            if (doc.isNull())
                return {};

            return TypeSet::fromJson(doc.object());
        }
    }
    return {};
}

bool TypeSetManager::isBuiltin(const QString &id) const
{
    for (const auto &t : m_typeSets) {
        if (t.id == id)
            return t.builtin;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Save / Delete
// ---------------------------------------------------------------------------

QString TypeSetManager::saveTypeSet(const TypeSet &typeSet)
{
    QString dir = userTypeSetsDir();
    QDir().mkpath(dir);

    QString id = typeSet.id;
    if (id.isEmpty()) {
        QString base = typeSet.name.toLower().replace(
            QRegularExpression(QStringLiteral("[^a-z0-9]+")),
            QStringLiteral("-"));
        if (base.isEmpty())
            base = QStringLiteral("typeset");
        id = base;
        // Ensure filename uniqueness
        QString path = dir + QLatin1Char('/') + id + QLatin1String(".json");
        int suffix = 1;
        while (QFile::exists(path)) {
            id = base + QLatin1Char('-') + QString::number(suffix++);
            path = dir + QLatin1Char('/') + id + QLatin1String(".json");
        }
    }

    // Check if we are overwriting an existing user type set
    bool found = false;
    for (auto &t : m_typeSets) {
        if (t.id == id) {
            if (t.builtin)
                return {}; // cannot overwrite built-in
            found = true;
            break;
        }
    }

    QString path = dir + QLatin1Char('/') + id + QLatin1String(".json");

    TypeSet toSave = typeSet;
    toSave.id = id;
    QJsonDocument doc(toSave.toJson());

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return {};

    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

    if (!found) {
        m_typeSets.append({id, toSave.name, path, false});
    } else {
        for (auto &t : m_typeSets) {
            if (t.id == id) {
                t.name = toSave.name;
                t.path = path;
                break;
            }
        }
    }

    Q_EMIT typeSetsChanged();
    return id;
}

bool TypeSetManager::deleteTypeSet(const QString &id)
{
    for (int i = 0; i < m_typeSets.size(); ++i) {
        if (m_typeSets[i].id == id) {
            if (m_typeSets[i].builtin)
                return false;

            if (!QFile::remove(m_typeSets[i].path))
                qWarning("TypeSetManager: failed to remove %s",
                         qPrintable(m_typeSets[i].path));
            m_typeSets.removeAt(i);
            Q_EMIT typeSetsChanged();
            return true;
        }
    }
    return false;
}
