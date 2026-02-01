#include "metadatastore.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QStandardPaths>

MetadataStore::MetadataStore(QObject *parent)
    : QObject(parent)
{
}

QString MetadataStore::metadataDir() const
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                  + QStringLiteral("/metadata");
    QDir().mkpath(dir);
    return dir;
}

QString MetadataStore::hashPath(const QString &filePath) const
{
    QByteArray hash = QCryptographicHash::hash(
        filePath.toUtf8(), QCryptographicHash::Sha256);
    return QString::fromLatin1(hash.toHex().left(16));
}

QString MetadataStore::metadataFilePath(const QString &filePath) const
{
    return metadataDir() + QLatin1Char('/') + hashPath(filePath)
           + QStringLiteral(".json");
}

QJsonObject MetadataStore::load(const QString &filePath) const
{
    QFile file(metadataFilePath(filePath));
    if (!file.open(QIODevice::ReadOnly))
        return {};

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    return doc.object();
}

void MetadataStore::save(const QString &filePath, const QJsonObject &metadata)
{
    QFile file(metadataFilePath(filePath));
    if (!file.open(QIODevice::WriteOnly))
        return;

    // Store the original file path in metadata for identification
    QJsonObject obj = metadata;
    obj[QStringLiteral("_filePath")] = filePath;

    file.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
}

void MetadataStore::remove(const QString &filePath)
{
    QFile::remove(metadataFilePath(filePath));
}

void MetadataStore::setValue(const QString &filePath, const QString &key,
                              const QJsonValue &value)
{
    QJsonObject obj = load(filePath);
    obj[key] = value;
    save(filePath, obj);
}

QJsonValue MetadataStore::value(const QString &filePath, const QString &key,
                                 const QJsonValue &defaultValue) const
{
    QJsonObject obj = load(filePath);
    if (obj.contains(key))
        return obj[key];
    return defaultValue;
}
