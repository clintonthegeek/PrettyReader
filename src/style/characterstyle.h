#ifndef PRETTYREADER_CHARACTERSTYLE_H
#define PRETTYREADER_CHARACTERSTYLE_H

#include <QColor>
#include <QFont>
#include <QString>
#include <QTextCharFormat>

class CharacterStyle
{
public:
    CharacterStyle() = default;
    explicit CharacterStyle(const QString &name);

    QString name() const { return m_name; }
    void setName(const QString &name) { m_name = name; }

    void setFontFamily(const QString &family) { m_fontFamily = family; m_hasFontFamily = true; }
    void setFontSize(qreal pts) { m_fontSize = pts; m_hasFontSize = true; }
    void setFontWeight(QFont::Weight w) { m_fontWeight = w; m_hasFontWeight = true; }
    void setFontItalic(bool on) { m_fontItalic = on; m_hasFontItalic = true; }
    void setFontUnderline(bool on) { m_fontUnderline = on; m_hasFontUnderline = true; }
    void setFontStrikeOut(bool on) { m_fontStrikeOut = on; m_hasFontStrikeOut = true; }
    void setForeground(const QColor &c) { m_foreground = c; m_hasForeground = true; }
    void setBackground(const QColor &c) { m_background = c; m_hasBackground = true; }
    void setLetterSpacing(qreal pts) { m_letterSpacing = pts; m_hasLetterSpacing = true; }

    // Apply to QTextCharFormat (merge -- does not reset existing properties)
    void applyFormat(QTextCharFormat &cf) const;

    // Inherit unset properties from parent
    void inheritFrom(const CharacterStyle &parent);

private:
    QString m_name;
    QString m_fontFamily;
    qreal m_fontSize = 0;
    QFont::Weight m_fontWeight = QFont::Normal;
    bool m_fontItalic = false;
    bool m_fontUnderline = false;
    bool m_fontStrikeOut = false;
    QColor m_foreground;
    QColor m_background;

    bool m_hasFontFamily = false;
    bool m_hasFontSize = false;
    bool m_hasFontWeight = false;
    bool m_hasFontItalic = false;
    bool m_hasFontUnderline = false;
    bool m_hasFontStrikeOut = false;
    bool m_hasForeground = false;
    bool m_hasBackground = false;
    qreal m_letterSpacing = 0;
    bool m_hasLetterSpacing = false;
};

#endif // PRETTYREADER_CHARACTERSTYLE_H
