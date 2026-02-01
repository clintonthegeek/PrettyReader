#ifndef PRETTYREADER_MARKDOWNHIGHLIGHTER_H
#define PRETTYREADER_MARKDOWNHIGHLIGHTER_H

#include <QRegularExpression>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>

class MarkdownHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    explicit MarkdownHighlighter(QTextDocument *parent = nullptr);

protected:
    void highlightBlock(const QString &text) override;

private:
    struct Rule {
        QRegularExpression pattern;
        QTextCharFormat format;
    };
    QList<Rule> m_rules;
};

#endif // PRETTYREADER_MARKDOWNHIGHLIGHTER_H
