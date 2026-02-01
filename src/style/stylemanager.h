#ifndef PRETTYREADER_STYLEMANAGER_H
#define PRETTYREADER_STYLEMANAGER_H

#include "characterstyle.h"
#include "paragraphstyle.h"

#include <QHash>
#include <QObject>
#include <QString>

class StyleManager : public QObject
{
    Q_OBJECT

public:
    explicit StyleManager(QObject *parent = nullptr);

    void addParagraphStyle(const ParagraphStyle &style);
    void addCharacterStyle(const CharacterStyle &style);

    ParagraphStyle *paragraphStyle(const QString &name);
    CharacterStyle *characterStyle(const QString &name);

    QStringList paragraphStyleNames() const;
    QStringList characterStyleNames() const;

signals:
    void stylesChanged();

private:
    QHash<QString, ParagraphStyle> m_paraStyles;
    QHash<QString, CharacterStyle> m_charStyles;
};

#endif // PRETTYREADER_STYLEMANAGER_H
