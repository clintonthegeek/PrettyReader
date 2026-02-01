#include "markdownhighlighter.h"

#include <QRegularExpression>

MarkdownHighlighter::MarkdownHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
{
    // Headings: # through ######
    {
        Rule rule;
        rule.pattern = QRegularExpression(QStringLiteral("^#{1,6}\\s.*$"));
        rule.format.setFontWeight(QFont::Bold);
        rule.format.setForeground(QColor(0x00, 0x55, 0x9e));
        m_rules.append(rule);
    }

    // Bold: **text** or __text__
    {
        Rule rule;
        rule.pattern = QRegularExpression(
            QStringLiteral(R"((\*\*|__)(.*?)\1)"));
        rule.format.setFontWeight(QFont::Bold);
        m_rules.append(rule);
    }

    // Italic: *text* or _text_
    {
        Rule rule;
        rule.pattern = QRegularExpression(
            QStringLiteral(R"((?<!\*)\*(?!\*)(.*?)(?<!\*)\*(?!\*))"));
        rule.format.setFontItalic(true);
        m_rules.append(rule);
    }

    // Inline code: `text`
    {
        Rule rule;
        rule.pattern = QRegularExpression(QStringLiteral("`[^`]+`"));
        rule.format.setForeground(QColor(0xc7, 0x25, 0x4e));
        rule.format.setBackground(QColor(0xf0, 0xf0, 0xf0));
        QFont mono(QStringLiteral("JetBrains Mono"));
        mono.setStyleHint(QFont::Monospace);
        rule.format.setFont(mono);
        m_rules.append(rule);
    }

    // Code fence: ``` or ~~~
    {
        Rule rule;
        rule.pattern = QRegularExpression(QStringLiteral("^(```|~~~).*$"));
        rule.format.setForeground(QColor(0x6a, 0x73, 0x7d));
        rule.format.setBackground(QColor(0xf6, 0xf8, 0xfa));
        m_rules.append(rule);
    }

    // Links: [text](url)
    {
        Rule rule;
        rule.pattern = QRegularExpression(
            QStringLiteral(R"(\[([^\]]+)\]\([^\)]+\))"));
        rule.format.setForeground(QColor(0x03, 0x66, 0xd6));
        rule.format.setFontUnderline(true);
        m_rules.append(rule);
    }

    // Images: ![alt](url)
    {
        Rule rule;
        rule.pattern = QRegularExpression(
            QStringLiteral(R"(!\[([^\]]*)\]\([^\)]+\))"));
        rule.format.setForeground(QColor(0x6f, 0x42, 0xc1));
        m_rules.append(rule);
    }

    // Blockquote: > text
    {
        Rule rule;
        rule.pattern = QRegularExpression(QStringLiteral("^>+\\s.*$"));
        rule.format.setForeground(QColor(0x6a, 0x73, 0x7d));
        rule.format.setFontItalic(true);
        m_rules.append(rule);
    }

    // List markers: -, *, +, or 1.
    {
        Rule rule;
        rule.pattern = QRegularExpression(
            QStringLiteral(R"(^\s*[-*+](?=\s)|^\s*\d+\.(?=\s))"));
        rule.format.setForeground(QColor(0xe3, 0x6c, 0x09));
        rule.format.setFontWeight(QFont::Bold);
        m_rules.append(rule);
    }

    // Horizontal rule: ---, ***, ___
    {
        Rule rule;
        rule.pattern = QRegularExpression(
            QStringLiteral(R"(^(\*{3,}|-{3,}|_{3,})\s*$)"));
        rule.format.setForeground(QColor(0xaa, 0xaa, 0xaa));
        m_rules.append(rule);
    }

    // Strikethrough: ~~text~~
    {
        Rule rule;
        rule.pattern = QRegularExpression(QStringLiteral("~~.+?~~"));
        rule.format.setFontStrikeOut(true);
        rule.format.setForeground(QColor(0x99, 0x99, 0x99));
        m_rules.append(rule);
    }
}

void MarkdownHighlighter::highlightBlock(const QString &text)
{
    for (const Rule &rule : m_rules) {
        auto it = rule.pattern.globalMatch(text);
        while (it.hasNext()) {
            auto match = it.next();
            setFormat(match.capturedStart(), match.capturedLength(),
                      rule.format);
        }
    }
}
