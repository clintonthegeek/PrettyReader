#include "paragraphstyle.h"
#include "fontfeatures.h"

#include <QFontDatabase>

static QFont::StyleHint guessStyleHint(const QString &family)
{
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

ParagraphStyle::ParagraphStyle(const QString &name)
    : m_name(name)
{
}

void ParagraphStyle::applyBlockFormat(QTextBlockFormat &bf) const
{
    if (m_hasAlignment)
        bf.setAlignment(m_alignment);
    if (m_hasSpaceBefore)
        bf.setTopMargin(m_spaceBefore);
    if (m_hasSpaceAfter)
        bf.setBottomMargin(m_spaceAfter);
    if (m_hasLeftMargin)
        bf.setLeftMargin(m_leftMargin);
    if (m_hasRightMargin)
        bf.setRightMargin(m_rightMargin);
    if (m_hasLineHeight)
        bf.setLineHeight(m_lineHeightPct, QTextBlockFormat::ProportionalHeight);
    if (m_hasBackground)
        bf.setBackground(m_background);
    if (m_headingLevel > 0)
        bf.setHeadingLevel(m_headingLevel);
    if (m_hasFirstLineIndent)
        bf.setTextIndent(m_firstLineIndent);
}

void ParagraphStyle::applyCharFormat(QTextCharFormat &cf) const
{
    if (m_hasFontFamily) {
        QFont font(m_fontFamily);
        font.setStyleHint(m_fontFamily.contains(QLatin1String("Mono"),
                              Qt::CaseInsensitive)
                          ? QFont::Monospace : QFont::Serif);
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
    if (m_hasForeground)
        cf.setForeground(m_foreground);
    if (m_hasWordSpacing) {
        QFont f = cf.font();
        f.setWordSpacing(m_wordSpacing);
        cf.setFont(f);
    }
    if (m_hasFontFeatures)
        FontFeatures::applyToCharFormat(cf, m_fontFeatures);
}

void ParagraphStyle::inheritFrom(const ParagraphStyle &parent)
{
    if (!m_hasAlignment)    { m_alignment = parent.m_alignment; m_hasAlignment = parent.m_hasAlignment; }
    if (!m_hasSpaceBefore)  { m_spaceBefore = parent.m_spaceBefore; m_hasSpaceBefore = parent.m_hasSpaceBefore; }
    if (!m_hasSpaceAfter)   { m_spaceAfter = parent.m_spaceAfter; m_hasSpaceAfter = parent.m_hasSpaceAfter; }
    if (!m_hasLeftMargin)   { m_leftMargin = parent.m_leftMargin; m_hasLeftMargin = parent.m_hasLeftMargin; }
    if (!m_hasRightMargin)  { m_rightMargin = parent.m_rightMargin; m_hasRightMargin = parent.m_hasRightMargin; }
    if (!m_hasLineHeight)   { m_lineHeightPct = parent.m_lineHeightPct; m_hasLineHeight = parent.m_hasLineHeight; }
    if (!m_hasBackground)   { m_background = parent.m_background; m_hasBackground = parent.m_hasBackground; }
    if (!m_hasFontFamily)   { m_fontFamily = parent.m_fontFamily; m_hasFontFamily = parent.m_hasFontFamily; }
    if (!m_hasFontSize)     { m_fontSize = parent.m_fontSize; m_hasFontSize = parent.m_hasFontSize; }
    if (!m_hasFontWeight)   { m_fontWeight = parent.m_fontWeight; m_hasFontWeight = parent.m_hasFontWeight; }
    if (!m_hasFontItalic)   { m_fontItalic = parent.m_fontItalic; m_hasFontItalic = parent.m_hasFontItalic; }
    if (!m_hasForeground)   { m_foreground = parent.m_foreground; m_hasForeground = parent.m_hasForeground; }
    if (!m_hasFirstLineIndent) { m_firstLineIndent = parent.m_firstLineIndent; m_hasFirstLineIndent = parent.m_hasFirstLineIndent; }
    if (!m_hasWordSpacing)  { m_wordSpacing = parent.m_wordSpacing; m_hasWordSpacing = parent.m_hasWordSpacing; }
    if (!m_hasFontFeatures) { m_fontFeatures = parent.m_fontFeatures; m_hasFontFeatures = parent.m_hasFontFeatures; }
}
