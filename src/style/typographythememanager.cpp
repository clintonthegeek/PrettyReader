/*
 * typographythememanager.cpp — Discovery/loading/saving for typography themes
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "typographythememanager.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStandardPaths>

TypographyThemeManager::TypographyThemeManager(QObject *parent)
    : QObject(parent)
{
    discoverThemes();
}

// ---------------------------------------------------------------------------
// Discovery
// ---------------------------------------------------------------------------

static QString userThemesDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
           + QLatin1String("/typography");
}

void TypographyThemeManager::discoverThemes()
{
    // Built-in themes bundled as Qt resources
    QDir resourceDir(QStringLiteral(":/typography"));
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
        if (root.value(QLatin1String("type")).toString()
            != QLatin1String("typographyTheme"))
            continue;

        QString id = root.value(QLatin1String("id")).toString();
        if (id.isEmpty())
            id = QFileInfo(entry).completeBaseName();
        QString name = root.value(QLatin1String("name")).toString(id);

        m_themes.append({id, name, path, true});
    }

    // User themes from XDG data directory
    QDir dir(userThemesDir());
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
            if (root.value(QLatin1String("type")).toString()
                != QLatin1String("typographyTheme"))
                continue;

            QString id = root.value(QLatin1String("id")).toString();
            if (id.isEmpty())
                id = QFileInfo(entry).completeBaseName();

            // Skip if a built-in already owns this ID
            bool alreadyKnown = false;
            for (const auto &existing : m_themes) {
                if (existing.id == id) { alreadyKnown = true; break; }
            }
            if (alreadyKnown)
                continue;

            QString name = root.value(QLatin1String("name")).toString(id);
            m_themes.append({id, name, path, false});
        }
    }
}

// ---------------------------------------------------------------------------
// Accessors
// ---------------------------------------------------------------------------

QStringList TypographyThemeManager::availableThemes() const
{
    QStringList ids;
    for (const auto &t : m_themes)
        ids.append(t.id);
    return ids;
}

QString TypographyThemeManager::themeName(const QString &id) const
{
    for (const auto &t : m_themes) {
        if (t.id == id)
            return t.name;
    }
    return id;
}

TypographyTheme TypographyThemeManager::theme(const QString &id) const
{
    for (const auto &t : m_themes) {
        if (t.id == id) {
            QFile file(t.path);
            if (!file.open(QIODevice::ReadOnly))
                return {};

            QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
            if (doc.isNull())
                return {};

            return TypographyTheme::fromJson(doc.object());
        }
    }
    return {};
}

bool TypographyThemeManager::isBuiltin(const QString &id) const
{
    for (const auto &t : m_themes) {
        if (t.id == id)
            return t.builtin;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Save / Delete
// ---------------------------------------------------------------------------

QString TypographyThemeManager::saveTheme(const TypographyTheme &theme)
{
    QString dir = userThemesDir();
    QDir().mkpath(dir);

    QString id = theme.id;
    if (id.isEmpty()) {
        QString base = theme.name.toLower().replace(
            QRegularExpression(QStringLiteral("[^a-z0-9]+")),
            QStringLiteral("-"));
        if (base.isEmpty())
            base = QStringLiteral("theme");
        id = base;
        // Ensure filename uniqueness
        QString path = dir + QLatin1Char('/') + id + QLatin1String(".json");
        int suffix = 1;
        while (QFile::exists(path)) {
            id = base + QLatin1Char('-') + QString::number(suffix++);
            path = dir + QLatin1Char('/') + id + QLatin1String(".json");
        }
    }

    // Check if we are overwriting an existing user theme
    bool found = false;
    for (auto &t : m_themes) {
        if (t.id == id) {
            if (t.builtin)
                return {}; // cannot overwrite built-in
            found = true;
            break;
        }
    }

    QString path = dir + QLatin1Char('/') + id + QLatin1String(".json");

    TypographyTheme toSave = theme;
    toSave.id = id;
    QJsonDocument doc(toSave.toJson());

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return {};

    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

    if (!found) {
        m_themes.append({id, toSave.name, path, false});
    } else {
        for (auto &t : m_themes) {
            if (t.id == id) {
                t.name = toSave.name;
                t.path = path;
                break;
            }
        }
    }

    Q_EMIT themesChanged();
    return id;
}

bool TypographyThemeManager::deleteTheme(const QString &id)
{
    for (int i = 0; i < m_themes.size(); ++i) {
        if (m_themes[i].id == id) {
            if (m_themes[i].builtin)
                return false;

            if (!QFile::remove(m_themes[i].path))
                qWarning("TypographyThemeManager: failed to remove %s",
                         qPrintable(m_themes[i].path));
            m_themes.removeAt(i);
            Q_EMIT themesChanged();
            return true;
        }
    }
    return false;
}
