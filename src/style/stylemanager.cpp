#include "stylemanager.h"

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
