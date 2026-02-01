#ifndef PRETTYREADER_DOCUMENTTAB_H
#define PRETTYREADER_DOCUMENTTAB_H

#include <QWidget>

class QPlainTextEdit;
class QStackedWidget;
class DocumentView;
class MarkdownHighlighter;

class DocumentTab : public QWidget
{
    Q_OBJECT

public:
    explicit DocumentTab(QWidget *parent = nullptr);

    DocumentView *documentView() const { return m_documentView; }
    QPlainTextEdit *sourceEditor() const { return m_sourceEditor; }

    void setFilePath(const QString &path) { m_filePath = path; }
    QString filePath() const { return m_filePath; }

    bool isSourceMode() const { return m_sourceMode; }
    void setSourceMode(bool source);

    // Get current source text (from editor)
    QString sourceText() const;
    // Set source text (into editor)
    void setSourceText(const QString &text);

Q_SIGNALS:
    void sourceModeChanged(bool sourceMode);

private:
    QStackedWidget *m_stack = nullptr;
    DocumentView *m_documentView = nullptr;
    QPlainTextEdit *m_sourceEditor = nullptr;
    MarkdownHighlighter *m_highlighter = nullptr;
    QString m_filePath;
    bool m_sourceMode = false;
};

#endif // PRETTYREADER_DOCUMENTTAB_H
