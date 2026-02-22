/*
 * pagetemplatemanager.cpp — Discovery/loading/saving for page templates
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "pagetemplatemanager.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStandardPaths>

PageTemplateManager::PageTemplateManager(QObject *parent)
    : QObject(parent)
{
    discoverTemplates();
}

// ---------------------------------------------------------------------------
// Discovery
// ---------------------------------------------------------------------------

static QString userTemplatesDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
           + QLatin1String("/templates");
}

void PageTemplateManager::discoverTemplates()
{
    // Built-in templates bundled as Qt resources
    QDir resourceDir(QStringLiteral(":/templates"));
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
            != QLatin1String("pageTemplate"))
            continue;

        QString id = root.value(QLatin1String("id")).toString();
        if (id.isEmpty())
            id = QFileInfo(entry).completeBaseName();
        QString name = root.value(QLatin1String("name")).toString(id);

        m_templates.append({id, name, path, true});
    }

    // User templates from XDG data directory
    QDir dir(userTemplatesDir());
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
                != QLatin1String("pageTemplate"))
                continue;

            QString id = root.value(QLatin1String("id")).toString();
            if (id.isEmpty())
                id = QFileInfo(entry).completeBaseName();

            // Skip if a built-in already owns this ID
            bool alreadyKnown = false;
            for (const auto &existing : m_templates) {
                if (existing.id == id) { alreadyKnown = true; break; }
            }
            if (alreadyKnown)
                continue;

            QString name = root.value(QLatin1String("name")).toString(id);
            m_templates.append({id, name, path, false});
        }
    }
}

// ---------------------------------------------------------------------------
// Accessors
// ---------------------------------------------------------------------------

QStringList PageTemplateManager::availableTemplates() const
{
    QStringList ids;
    for (const auto &t : m_templates)
        ids.append(t.id);
    return ids;
}

QString PageTemplateManager::templateName(const QString &id) const
{
    for (const auto &t : m_templates) {
        if (t.id == id)
            return t.name;
    }
    return id;
}

PageTemplate PageTemplateManager::pageTemplate(const QString &id) const
{
    for (const auto &t : m_templates) {
        if (t.id == id) {
            QFile file(t.path);
            if (!file.open(QIODevice::ReadOnly))
                return {};

            QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
            if (doc.isNull())
                return {};

            return PageTemplate::fromJson(doc.object());
        }
    }
    return {};
}

bool PageTemplateManager::isBuiltin(const QString &id) const
{
    for (const auto &t : m_templates) {
        if (t.id == id)
            return t.builtin;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Save / Delete
// ---------------------------------------------------------------------------

QString PageTemplateManager::saveTemplate(const PageTemplate &tmpl)
{
    QString dir = userTemplatesDir();
    QDir().mkpath(dir);

    QString id = tmpl.id;
    if (id.isEmpty()) {
        QString base = tmpl.name.toLower().replace(
            QRegularExpression(QStringLiteral("[^a-z0-9]+")),
            QStringLiteral("-"));
        if (base.isEmpty())
            base = QStringLiteral("template");
        id = base;
        // Ensure filename uniqueness
        QString path = dir + QLatin1Char('/') + id + QLatin1String(".json");
        int suffix = 1;
        while (QFile::exists(path)) {
            id = base + QLatin1Char('-') + QString::number(suffix++);
            path = dir + QLatin1Char('/') + id + QLatin1String(".json");
        }
    }

    // Check if we are overwriting an existing user template
    bool found = false;
    for (auto &t : m_templates) {
        if (t.id == id) {
            if (t.builtin)
                return {}; // cannot overwrite built-in
            found = true;
            break;
        }
    }

    QString path = dir + QLatin1Char('/') + id + QLatin1String(".json");

    PageTemplate toSave = tmpl;
    toSave.id = id;
    QJsonDocument doc(toSave.toJson());

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return {};

    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

    if (!found) {
        m_templates.append({id, toSave.name, path, false});
    } else {
        for (auto &t : m_templates) {
            if (t.id == id) {
                t.name = toSave.name;
                t.path = path;
                break;
            }
        }
    }

    Q_EMIT templatesChanged();
    return id;
}

bool PageTemplateManager::deleteTemplate(const QString &id)
{
    for (int i = 0; i < m_templates.size(); ++i) {
        if (m_templates[i].id == id) {
            if (m_templates[i].builtin)
                return false;

            if (!QFile::remove(m_templates[i].path))
                qWarning("PageTemplateManager: failed to remove %s",
                         qPrintable(m_templates[i].path));
            m_templates.removeAt(i);
            Q_EMIT templatesChanged();
            return true;
        }
    }
    return false;
}
