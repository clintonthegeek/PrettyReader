#ifndef PRETTYREADER_PARAGRAPHSTYLE_H
#define PRETTYREADER_PARAGRAPHSTYLE_H

#include <QFont>
#include <QString>
#include <QTextBlockFormat>
#include <QTextCharFormat>

class ParagraphStyle
{
public:
    ParagraphStyle() = default;
    explicit ParagraphStyle(const QString &name);

    QString name() const { return m_name; }
    void setName(const QString &name) { m_name = name; }

    // Block formatting
    void setAlignment(Qt::Alignment align) { m_alignment = align; m_hasAlignment = true; }
    void setSpaceBefore(qreal pts) { m_spaceBefore = pts; m_hasSpaceBefore = true; }
    void setSpaceAfter(qreal pts) { m_spaceAfter = pts; m_hasSpaceAfter = true; }
    void setLeftMargin(qreal pts) { m_leftMargin = pts; m_hasLeftMargin = true; }
    void setRightMargin(qreal pts) { m_rightMargin = pts; m_hasRightMargin = true; }
    void setLineHeightPercent(int pct) { m_lineHeightPct = pct; m_hasLineHeight = true; }
    void setBackground(const QColor &c) { m_background = c; m_hasBackground = true; }
    void setHeadingLevel(int level) { m_headingLevel = level; }
    void setFirstLineIndent(qreal pts) { m_firstLineIndent = pts; m_hasFirstLineIndent = true; }
    void setWordSpacing(qreal pts) { m_wordSpacing = pts; m_hasWordSpacing = true; }

    // Getters for querying explicit state
    bool hasExplicitAlignment() const { return m_hasAlignment; }
    bool hasExplicitBackground() const { return m_hasBackground; }
    bool hasExplicitForeground() const { return m_hasForeground; }
    QColor background() const { return m_background; }
    QColor foreground() const { return m_foreground; }
    int headingLevel() const { return m_headingLevel; }

    // Character formatting (inherited by text in this paragraph)
    void setFontFamily(const QString &family) { m_fontFamily = family; m_hasFontFamily = true; }
    void setFontSize(qreal pts) { m_fontSize = pts; m_hasFontSize = true; }
    void setFontWeight(QFont::Weight w) { m_fontWeight = w; m_hasFontWeight = true; }
    void setFontItalic(bool on) { m_fontItalic = on; m_hasFontItalic = true; }
    void setForeground(const QColor &c) { m_foreground = c; m_hasForeground = true; }

    // Apply to QTextBlockFormat + QTextCharFormat
    void applyBlockFormat(QTextBlockFormat &bf) const;
    void applyCharFormat(QTextCharFormat &cf) const;

    // Inherit unset properties from parent
    void inheritFrom(const ParagraphStyle &parent);

private:
    QString m_name;

    // Block properties
    Qt::Alignment m_alignment = Qt::AlignLeft;
    qreal m_spaceBefore = 0;
    qreal m_spaceAfter = 0;
    qreal m_leftMargin = 0;
    qreal m_rightMargin = 0;
    int m_lineHeightPct = 100;
    QColor m_background;
    int m_headingLevel = 0;

    bool m_hasAlignment = false;
    bool m_hasSpaceBefore = false;
    bool m_hasSpaceAfter = false;
    bool m_hasLeftMargin = false;
    bool m_hasRightMargin = false;
    bool m_hasLineHeight = false;
    bool m_hasBackground = false;
    qreal m_firstLineIndent = 0;
    bool m_hasFirstLineIndent = false;
    qreal m_wordSpacing = 0;
    bool m_hasWordSpacing = false;

    // Char properties
    QString m_fontFamily;
    qreal m_fontSize = 0;
    QFont::Weight m_fontWeight = QFont::Normal;
    bool m_fontItalic = false;
    QColor m_foreground;

    bool m_hasFontFamily = false;
    bool m_hasFontSize = false;
    bool m_hasFontWeight = false;
    bool m_hasFontItalic = false;
    bool m_hasForeground = false;
};

#endif // PRETTYREADER_PARAGRAPHSTYLE_H
