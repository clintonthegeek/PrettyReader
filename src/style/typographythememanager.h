/*
 * typographythememanager.h — Discovery/loading/saving for typography themes
 *
 * Scans built-in Qt resources (:/typography/) and the user data
 * directory for JSON typography theme files and presents them by ID.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef PRETTYREADER_TYPOGRAPHYTHEMEMANAGER_H
#define PRETTYREADER_TYPOGRAPHYTHEMEMANAGER_H

#include <QObject>
#include <QString>
#include <QStringList>

#include "typographytheme.h"

class TypographyThemeManager : public QObject
{
    Q_OBJECT

public:
    explicit TypographyThemeManager(QObject *parent = nullptr);

    /// List of all available theme IDs (built-in + user).
    QStringList availableThemes() const;

    /// Display name for a theme ID.
    QString themeName(const QString &id) const;

    /// Load a typography theme by ID.
    TypographyTheme theme(const QString &id) const;

    /// Save a user typography theme. Returns the assigned ID.
    QString saveTheme(const TypographyTheme &theme);

    /// Delete a user typography theme.
    bool deleteTheme(const QString &id);

    /// Whether a theme is built-in (read-only).
    bool isBuiltin(const QString &id) const;

Q_SIGNALS:
    void themesChanged();

private:
    void discoverThemes();

    struct ThemeInfo {
        QString id;
        QString name;
        QString path;
        bool builtin = false;
    };
    QList<ThemeInfo> m_themes;
};

#endif // PRETTYREADER_TYPOGRAPHYTHEMEMANAGER_H
