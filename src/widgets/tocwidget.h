#ifndef PRETTYREADER_TOCWIDGET_H
#define PRETTYREADER_TOCWIDGET_H

#include <QWidget>

#include "contentmodel.h"
#include "layoutengine.h"

class QTreeWidget;
class QTreeWidgetItem;
class QTextDocument;

class TocWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TocWidget(QWidget *parent = nullptr);

    void buildFromDocument(QTextDocument *document);
    void buildFromContentModel(const Content::Document &doc,
                               const QList<Layout::SourceMapEntry> &sourceMap);
    void clear();
    void highlightHeading(int index);

Q_SIGNALS:
    void headingClicked(int blockNumber);
    void headingNavigate(int page, qreal yOffset);

private Q_SLOTS:
    void onItemClicked(QTreeWidgetItem *item, int column);

private:
    QTreeWidget *m_treeWidget = nullptr;
    QList<QTreeWidgetItem *> m_flatHeadingItems;
};

#endif // PRETTYREADER_TOCWIDGET_H
