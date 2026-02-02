#include "characterstyle.h"

#include <QFontDatabase>

static QFont::StyleHint guessStyleHint(const QString &family)
{
    // Check name patterns for monospace fonts
    static const char *monoPatterns[] = {
        "Mono", "Code", "Courier", "Console", "Consolas",
        "Hack", "Inconsolata", "Menlo", "Monaco", "Terminal"
    };
    for (const char *p : monoPatterns) {
        if (family.contains(QLatin1String(p), Qt::CaseInsensitive))
            return QFont::Monospace;
    }
    if (QFontDatabase::isFixedPitch(family))
        return QFont::Monospace;
    if (family.contains(QLatin1String("Serif"), Qt::CaseInsensitive))
        return QFont::Serif;
    return QFont::SansSerif;
}

CharacterStyle::CharacterStyle(const QString &name)
    : m_name(name)
{
}

void CharacterStyle::applyFormat(QTextCharFormat &cf) const
{
    if (m_hasFontFamily) {
        QFont font(m_fontFamily);
        font.setStyleHint(guessStyleHint(m_fontFamily));
        if (m_hasFontSize)
            font.setPointSizeF(m_fontSize);
        if (m_hasFontWeight)
            font.setWeight(m_fontWeight);
        if (m_hasFontItalic)
            font.setItalic(m_fontItalic);
        cf.setFont(font);
    } else {
        if (m_hasFontSize)
            cf.setFontPointSize(m_fontSize);
        if (m_hasFontWeight)
            cf.setFontWeight(m_fontWeight);
        if (m_hasFontItalic)
            cf.setFontItalic(m_fontItalic);
    }
    if (m_hasFontUnderline)
        cf.setFontUnderline(m_fontUnderline);
    if (m_hasFontStrikeOut)
        cf.setFontStrikeOut(m_fontStrikeOut);
    if (m_hasForeground)
        cf.setForeground(m_foreground);
    if (m_hasBackground)
        cf.setBackground(m_background);
    if (m_hasLetterSpacing)
        cf.setFontLetterSpacing(m_letterSpacing);
}

void CharacterStyle::inheritFrom(const CharacterStyle &parent)
{
    if (!m_hasFontFamily)    { m_fontFamily = parent.m_fontFamily; m_hasFontFamily = parent.m_hasFontFamily; }
    if (!m_hasFontSize)      { m_fontSize = parent.m_fontSize; m_hasFontSize = parent.m_hasFontSize; }
    if (!m_hasFontWeight)    { m_fontWeight = parent.m_fontWeight; m_hasFontWeight = parent.m_hasFontWeight; }
    if (!m_hasFontItalic)    { m_fontItalic = parent.m_fontItalic; m_hasFontItalic = parent.m_hasFontItalic; }
    if (!m_hasFontUnderline) { m_fontUnderline = parent.m_fontUnderline; m_hasFontUnderline = parent.m_hasFontUnderline; }
    if (!m_hasFontStrikeOut) { m_fontStrikeOut = parent.m_fontStrikeOut; m_hasFontStrikeOut = parent.m_hasFontStrikeOut; }
    if (!m_hasForeground)    { m_foreground = parent.m_foreground; m_hasForeground = parent.m_hasForeground; }
    if (!m_hasBackground)    { m_background = parent.m_background; m_hasBackground = parent.m_hasBackground; }
    if (!m_hasLetterSpacing) { m_letterSpacing = parent.m_letterSpacing; m_hasLetterSpacing = parent.m_hasLetterSpacing; }
}
