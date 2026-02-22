/*
 * typesetmanager.h — Discovery/loading/saving for type sets
 *
 * Scans built-in Qt resources (:/typesets/) and the user data
 * directory for JSON type set files and presents them by ID.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef PRETTYREADER_TYPESETMANAGER_H
#define PRETTYREADER_TYPESETMANAGER_H

#include <QObject>
#include <QString>
#include <QStringList>

#include "typeset.h"

class TypeSetManager : public QObject
{
    Q_OBJECT

public:
    explicit TypeSetManager(QObject *parent = nullptr);

    /// List of all available type set IDs (built-in + user).
    QStringList availableTypeSets() const;

    /// Display name for a type set ID.
    QString typeSetName(const QString &id) const;

    /// Load a type set by ID.
    TypeSet typeSet(const QString &id) const;

    /// Save a user type set. Returns the assigned ID.
    QString saveTypeSet(const TypeSet &typeSet);

    /// Delete a user type set.
    bool deleteTypeSet(const QString &id);

    /// Whether a type set is built-in (read-only).
    bool isBuiltin(const QString &id) const;

Q_SIGNALS:
    void typeSetsChanged();

private:
    void discoverTypeSets();

    struct TypeSetInfo {
        QString id;
        QString name;
        QString path;
        bool builtin = false;
    };
    QList<TypeSetInfo> m_typeSets;
};

#endif // PRETTYREADER_TYPESETMANAGER_H
