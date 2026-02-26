/*
 * pagetemplatemanager.cpp — Discovery/loading/saving for page templates
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "pagetemplatemanager.h"

#include <QStandardPaths>

static QString userTemplatesDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
           + QLatin1String("/templates");
}

PageTemplateManager::PageTemplateManager(QObject *parent)
    : QObject(parent)
{
    m_store.discover(
        QStringLiteral("templates"),
        [](const QJsonObject &root) {
            return root.value(QLatin1String("type")).toString()
                   == QLatin1String("pageTemplate");
        },
        {userTemplatesDir()});
}

PageTemplate PageTemplateManager::pageTemplate(const QString &id) const
{
    QJsonObject json = m_store.loadJson(id);
    return json.isEmpty() ? PageTemplate{} : PageTemplate::fromJson(json);
}

QString PageTemplateManager::saveTemplate(const PageTemplate &tmpl)
{
    PageTemplate toSave = tmpl;
    if (toSave.id.isEmpty())
        toSave.id = QStringLiteral("placeholder");
    QString id = m_store.save(tmpl.id, tmpl.name,
                              toSave.toJson(), QStringLiteral("template"));
    if (!id.isEmpty())
        Q_EMIT templatesChanged();
    return id;
}

bool PageTemplateManager::deleteTemplate(const QString &id)
{
    if (m_store.remove(id, "PageTemplateManager")) {
        Q_EMIT templatesChanged();
        return true;
    }
    return false;
}
