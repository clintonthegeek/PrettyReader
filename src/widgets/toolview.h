#ifndef PRETTYREADER_TOOLVIEW_H
#define PRETTYREADER_TOOLVIEW_H

#include <QFrame>

class QLabel;
class QToolButton;

class ToolView : public QFrame
{
    Q_OBJECT

public:
    explicit ToolView(const QString &title, QWidget *content,
                      QWidget *parent = nullptr);

Q_SIGNALS:
    void closeRequested();

private:
    QWidget *m_content;
    QLabel *m_titleLabel;
};

#endif // PRETTYREADER_TOOLVIEW_H
