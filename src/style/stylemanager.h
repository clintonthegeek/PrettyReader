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

    const QHash<QString, ParagraphStyle> &paragraphStyles() const { return m_paraStyles; }
    const QHash<QString, CharacterStyle> &characterStyles() const { return m_charStyles; }

    QStringList paragraphStyleNames() const;
    QStringList characterStyleNames() const;

    // Resolve a style by walking its parent chain.
    // Returns a fully-resolved copy with all inherited properties filled in.
    ParagraphStyle resolvedParagraphStyle(const QString &name);
    CharacterStyle resolvedCharacterStyle(const QString &name);

    // Get ordered ancestor list for a style (for tree display)
    QStringList paragraphStyleAncestors(const QString &name);
    QStringList characterStyleAncestors(const QString &name);

    // Deep-copy this style manager
    StyleManager *clone(QObject *parent = nullptr) const;

signals:
    void stylesChanged();

private:
    QHash<QString, ParagraphStyle> m_paraStyles;
    QHash<QString, CharacterStyle> m_charStyles;
};

#endif // PRETTYREADER_STYLEMANAGER_H
