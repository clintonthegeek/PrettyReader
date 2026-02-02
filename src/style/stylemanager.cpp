#include "stylemanager.h"

#include <QSet>

StyleManager::StyleManager(QObject *parent)
    : QObject(parent)
{
}

void StyleManager::addParagraphStyle(const ParagraphStyle &style)
{
    m_paraStyles.insert(style.name(), style);
}

void StyleManager::addCharacterStyle(const CharacterStyle &style)
{
    m_charStyles.insert(style.name(), style);
}

ParagraphStyle *StyleManager::paragraphStyle(const QString &name)
{
    auto it = m_paraStyles.find(name);
    return it != m_paraStyles.end() ? &it.value() : nullptr;
}

CharacterStyle *StyleManager::characterStyle(const QString &name)
{
    auto it = m_charStyles.find(name);
    return it != m_charStyles.end() ? &it.value() : nullptr;
}

QStringList StyleManager::paragraphStyleNames() const
{
    return m_paraStyles.keys();
}

QStringList StyleManager::characterStyleNames() const
{
    return m_charStyles.keys();
}

ParagraphStyle StyleManager::resolvedParagraphStyle(const QString &name)
{
    auto it = m_paraStyles.find(name);
    if (it == m_paraStyles.end())
        return ParagraphStyle(name);

    ParagraphStyle resolved = it.value();
    QSet<QString> visited;
    visited.insert(name);

    QString parentName = resolved.parentStyleName();
    while (!parentName.isEmpty() && !visited.contains(parentName)) {
        visited.insert(parentName);
        auto pit = m_paraStyles.find(parentName);
        if (pit == m_paraStyles.end())
            break;
        resolved.inheritFrom(pit.value());
        parentName = pit.value().parentStyleName();
    }

    return resolved;
}

CharacterStyle StyleManager::resolvedCharacterStyle(const QString &name)
{
    auto it = m_charStyles.find(name);
    if (it == m_charStyles.end())
        return CharacterStyle(name);

    CharacterStyle resolved = it.value();
    QSet<QString> visited;
    visited.insert(name);

    QString parentName = resolved.parentStyleName();
    while (!parentName.isEmpty() && !visited.contains(parentName)) {
        visited.insert(parentName);
        auto pit = m_charStyles.find(parentName);
        if (pit == m_charStyles.end())
            break;
        resolved.inheritFrom(pit.value());
        parentName = pit.value().parentStyleName();
    }

    return resolved;
}

QStringList StyleManager::paragraphStyleAncestors(const QString &name)
{
    QStringList ancestors;
    QSet<QString> visited;
    visited.insert(name);

    auto it = m_paraStyles.find(name);
    if (it == m_paraStyles.end())
        return ancestors;

    QString parentName = it.value().parentStyleName();
    while (!parentName.isEmpty() && !visited.contains(parentName)) {
        visited.insert(parentName);
        ancestors.append(parentName);
        auto pit = m_paraStyles.find(parentName);
        if (pit == m_paraStyles.end())
            break;
        parentName = pit.value().parentStyleName();
    }

    return ancestors;
}

QStringList StyleManager::characterStyleAncestors(const QString &name)
{
    QStringList ancestors;
    QSet<QString> visited;
    visited.insert(name);

    auto it = m_charStyles.find(name);
    if (it == m_charStyles.end())
        return ancestors;

    QString parentName = it.value().parentStyleName();
    while (!parentName.isEmpty() && !visited.contains(parentName)) {
        visited.insert(parentName);
        ancestors.append(parentName);
        auto pit = m_charStyles.find(parentName);
        if (pit == m_charStyles.end())
            break;
        parentName = pit.value().parentStyleName();
    }

    return ancestors;
}

void StyleManager::addTableStyle(const TableStyle &style)
{
    m_tableStyles.insert(style.name(), style);
}

TableStyle *StyleManager::tableStyle(const QString &name)
{
    auto it = m_tableStyles.find(name);
    return it != m_tableStyles.end() ? &it.value() : nullptr;
}

QStringList StyleManager::tableStyleNames() const
{
    return m_tableStyles.keys();
}

StyleManager *StyleManager::clone(QObject *parent) const
{
    auto *copy = new StyleManager(parent);
    copy->m_paraStyles = m_paraStyles;
    copy->m_charStyles = m_charStyles;
    copy->m_tableStyles = m_tableStyles;
    copy->m_footnoteStyle = m_footnoteStyle;
    return copy;
}
