/*
 * typesetmanager.cpp — Discovery/loading/saving for type sets
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "typesetmanager.h"

#include <QStandardPaths>

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

TypeSetManager::TypeSetManager(QObject *parent)
    : QObject(parent)
{
    m_store.discover(
        QStringLiteral("typesets"),
        [](const QJsonObject &root) {
            QString type = root.value(QLatin1String("type")).toString();
            return type == QLatin1String("typeSet")
                   || type == QLatin1String("typographyTheme");
        },
        {userTypeSetsDir(), legacyUserTypeSetsDir()});
}

TypeSet TypeSetManager::typeSet(const QString &id) const
{
    QJsonObject json = m_store.loadJson(id);
    return json.isEmpty() ? TypeSet{} : TypeSet::fromJson(json);
}

QString TypeSetManager::saveTypeSet(const TypeSet &typeSet)
{
    TypeSet toSave = typeSet;
    if (toSave.id.isEmpty())
        toSave.id = QStringLiteral("placeholder");
    QString id = m_store.save(typeSet.id, typeSet.name,
                              toSave.toJson(), QStringLiteral("typeset"));
    if (!id.isEmpty())
        Q_EMIT typeSetsChanged();
    return id;
}

bool TypeSetManager::deleteTypeSet(const QString &id)
{
    if (m_store.remove(id, "TypeSetManager")) {
        Q_EMIT typeSetsChanged();
        return true;
    }
    return false;
}
