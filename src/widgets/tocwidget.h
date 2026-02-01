#ifndef PRETTYREADER_TOCWIDGET_H
#define PRETTYREADER_TOCWIDGET_H

#include <QWidget>

class QTreeWidget;
class QTreeWidgetItem;
class QTextDocument;

class TocWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TocWidget(QWidget *parent = nullptr);

    void buildFromDocument(QTextDocument *document);
    void clear();

Q_SIGNALS:
    void headingClicked(int blockNumber);

private Q_SLOTS:
    void onItemClicked(QTreeWidgetItem *item, int column);

private:
    QTreeWidget *m_treeWidget = nullptr;
};

#endif // PRETTYREADER_TOCWIDGET_H
