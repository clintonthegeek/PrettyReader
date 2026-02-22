/*
 * pagetemplatemanager.h — Discovery/loading/saving for page templates
 *
 * Scans built-in Qt resources (:/templates/) and the user data
 * directory for JSON page template files and presents them by ID.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef PRETTYREADER_PAGETEMPLATEMANAGER_H
#define PRETTYREADER_PAGETEMPLATEMANAGER_H

#include <QObject>
#include <QString>
#include <QStringList>

#include "pagetemplate.h"

class PageTemplateManager : public QObject
{
    Q_OBJECT

public:
    explicit PageTemplateManager(QObject *parent = nullptr);

    /// List of all available template IDs (built-in + user).
    QStringList availableTemplates() const;

    /// Display name for a template ID.
    QString templateName(const QString &id) const;

    /// Load a page template by ID.
    PageTemplate pageTemplate(const QString &id) const;

    /// Save a user page template. Returns the assigned ID.
    QString saveTemplate(const PageTemplate &tmpl);

    /// Delete a user page template.
    bool deleteTemplate(const QString &id);

    /// Whether a template is built-in (read-only).
    bool isBuiltin(const QString &id) const;

Q_SIGNALS:
    void templatesChanged();

private:
    void discoverTemplates();

    struct TemplateInfo {
        QString id;
        QString name;
        QString path;
        bool builtin = false;
    };
    QList<TemplateInfo> m_templates;
};

#endif // PRETTYREADER_PAGETEMPLATEMANAGER_H
