#ifndef PRETTYREADER_DROPTARGETLINEEDIT_H
#define PRETTYREADER_DROPTARGETLINEEDIT_H

#include <QLineEdit>

class QDragEnterEvent;
class QDropEvent;

class DropTargetLineEdit : public QLineEdit
{
    Q_OBJECT

public:
    explicit DropTargetLineEdit(QWidget *parent = nullptr);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
};

#endif // PRETTYREADER_DROPTARGETLINEEDIT_H
