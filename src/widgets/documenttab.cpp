#include "documenttab.h"
#include "documentview.h"
#include "markdownhighlighter.h"

#include <QFont>
#include <QPlainTextEdit>
#include <QStackedWidget>
#include <QVBoxLayout>

DocumentTab::DocumentTab(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_stack = new QStackedWidget(this);
    layout->addWidget(m_stack);

    // Reader view (index 0)
    m_documentView = new DocumentView(this);
    m_stack->addWidget(m_documentView);

    // Source editor (index 1)
    m_sourceEditor = new QPlainTextEdit(this);
    m_sourceEditor->setLineWrapMode(QPlainTextEdit::NoWrap);
    QFont mono(QStringLiteral("JetBrains Mono"), 11);
    mono.setStyleHint(QFont::Monospace);
    m_sourceEditor->setFont(mono);

    m_highlighter = new MarkdownHighlighter(m_sourceEditor->document());

    m_stack->addWidget(m_sourceEditor);
    m_stack->setCurrentIndex(0); // Start in reader mode
}

void DocumentTab::setSourceMode(bool source)
{
    if (m_sourceMode == source)
        return;
    m_sourceMode = source;
    m_stack->setCurrentIndex(source ? 1 : 0);
    Q_EMIT sourceModeChanged(source);
}

QString DocumentTab::sourceText() const
{
    return m_sourceEditor->toPlainText();
}

void DocumentTab::setSourceText(const QString &text)
{
    m_sourceEditor->setPlainText(text);
}
