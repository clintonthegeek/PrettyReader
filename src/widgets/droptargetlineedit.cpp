#include "droptargetlineedit.h"

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>

DropTargetLineEdit::DropTargetLineEdit(QWidget *parent)
    : QLineEdit(parent)
{
    setAcceptDrops(true);
}

void DropTargetLineEdit::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasText())
        event->acceptProposedAction();
    else
        QLineEdit::dragEnterEvent(event);
}

void DropTargetLineEdit::dragMoveEvent(QDragMoveEvent *event)
{
    if (event->mimeData()->hasText())
        event->acceptProposedAction();
    else
        QLineEdit::dragMoveEvent(event);
}

void DropTargetLineEdit::dropEvent(QDropEvent *event)
{
    if (event->mimeData()->hasText()) {
        int pos = cursorPositionAt(event->position().toPoint());
        QString current = text();
        current.insert(pos, event->mimeData()->text());
        setText(current);
        setCursorPosition(pos + event->mimeData()->text().length());
        event->acceptProposedAction();
    } else {
        QLineEdit::dropEvent(event);
    }
}
