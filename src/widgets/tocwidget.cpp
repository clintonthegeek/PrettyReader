#include "tocwidget.h"

#include <QHeaderView>
#include <QTextBlock>
#include <QTextDocument>
#include <QTreeWidget>
#include <QVBoxLayout>

TocWidget::TocWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_treeWidget = new QTreeWidget(this);
    m_treeWidget->setHeaderHidden(true);
    m_treeWidget->setRootIsDecorated(true);
    m_treeWidget->setIndentation(16);
    layout->addWidget(m_treeWidget);

    connect(m_treeWidget, &QTreeWidget::itemClicked,
            this, &TocWidget::onItemClicked);
}

void TocWidget::buildFromDocument(QTextDocument *document)
{
    m_treeWidget->clear();
    if (!document)
        return;

    // Stack of parent items for nesting headings
    // Index 0 = invisible root, index 1-6 = last item at that heading level
    QTreeWidgetItem *parents[7] = {};

    for (QTextBlock block = document->begin();
         block != document->end(); block = block.next()) {
        int level = block.blockFormat().headingLevel();
        if (level < 1 || level > 6)
            continue;

        QString text = block.text().trimmed();
        if (text.isEmpty())
            continue;

        auto *item = new QTreeWidgetItem();
        item->setText(0, text);
        item->setData(0, Qt::UserRole, block.blockNumber());

        // Find the appropriate parent
        QTreeWidgetItem *parent = nullptr;
        for (int i = level - 1; i >= 1; --i) {
            if (parents[i]) {
                parent = parents[i];
                break;
            }
        }

        if (parent) {
            parent->addChild(item);
        } else {
            m_treeWidget->addTopLevelItem(item);
        }

        parents[level] = item;
        // Clear deeper level parents
        for (int i = level + 1; i <= 6; ++i)
            parents[i] = nullptr;
    }

    m_treeWidget->expandAll();
}

void TocWidget::clear()
{
    m_treeWidget->clear();
}

void TocWidget::onItemClicked(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column);
    bool ok;
    int blockNumber = item->data(0, Qt::UserRole).toInt(&ok);
    if (ok)
        Q_EMIT headingClicked(blockNumber);
}
