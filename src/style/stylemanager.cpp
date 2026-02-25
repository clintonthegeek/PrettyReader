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

    // Cross-type linkage: fill unset char properties from the referenced
    // character style. Applied BEFORE the paragraph parent chain so the
    // character style's properties (e.g. Code → monospace font) win over
    // inherited paragraph defaults (e.g. Default Paragraph Style → serif).
    QString baseCharName = it.value().baseCharacterStyleName();
    if (!baseCharName.isEmpty()) {
        CharacterStyle charResolved = resolvedCharacterStyle(baseCharName);
        if (!resolved.hasFontFamily() && charResolved.hasFontFamily())
            resolved.setFontFamily(charResolved.fontFamily());
        if (!resolved.hasFontSize() && charResolved.hasFontSize())
            resolved.setFontSize(charResolved.fontSize());
        if (!resolved.hasFontWeight() && charResolved.hasFontWeight())
            resolved.setFontWeight(charResolved.fontWeight());
        if (!resolved.hasFontItalic() && charResolved.hasFontItalic())
            resolved.setFontItalic(charResolved.fontItalic());
        if (!resolved.hasForeground() && charResolved.hasForeground())
            resolved.setForeground(charResolved.foreground());
        if (!resolved.hasFontFeatures() && charResolved.hasFontFeatures())
            resolved.setFontFeatures(charResolved.fontFeatures());
    }

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
