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
    m_headingsByLine.clear();
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

void TocWidget::buildFromContentModel(const Content::Document &doc,
                                       const QList<Layout::SourceMapEntry> &sourceMap)
{
    m_treeWidget->clear();
    m_headingsByLine.clear();

    QTreeWidgetItem *parents[7] = {};

    for (const auto &block : doc.blocks) {
        const Content::Heading *heading = std::get_if<Content::Heading>(&block);
        if (!heading)
            continue;

        int level = heading->level;
        if (level < 1 || level > 6)
            continue;

        // Extract heading text from inlines
        QString text;
        for (const auto &node : heading->inlines) {
            std::visit([&](const auto &n) {
                using T = std::decay_t<decltype(n)>;
                if constexpr (std::is_same_v<T, Content::TextRun>)
                    text += n.text;
                else if constexpr (std::is_same_v<T, Content::InlineCode>)
                    text += n.text;
                else if constexpr (std::is_same_v<T, Content::Link>)
                    text += n.text;
            }, node);
        }
        text = text.trimmed();
        if (text.isEmpty())
            continue;

        // Find page and y-offset from source map
        int page = 0;
        qreal yOffset = 0;
        if (heading->source.startLine > 0) {
            for (const auto &entry : sourceMap) {
                if (entry.startLine == heading->source.startLine
                    && entry.endLine == heading->source.endLine) {
                    page = entry.pageNumber;
                    yOffset = entry.rect.top();
                    break;
                }
            }
        }

        auto *item = new QTreeWidgetItem();
        item->setText(0, text);
        // Store page in UserRole and y-offset in UserRole+1
        item->setData(0, Qt::UserRole, page);
        item->setData(0, Qt::UserRole + 1, yOffset);

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
        if (heading->source.startLine > 0)
            m_headingsByLine.insert(heading->source.startLine, item);
        for (int i = level + 1; i <= 6; ++i)
            parents[i] = nullptr;
    }

    m_treeWidget->expandAll();
}

void TocWidget::clear()
{
    m_treeWidget->clear();
    m_headingsByLine.clear();
}

void TocWidget::onItemClicked(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column);

    // Check if this item has y-offset data (content model path)
    QVariant yVar = item->data(0, Qt::UserRole + 1);
    if (yVar.isValid()) {
        int page = item->data(0, Qt::UserRole).toInt();
        qreal yOffset = yVar.toDouble();
        Q_EMIT headingNavigate(page, yOffset);
        return;
    }

    // Legacy path: block number
    bool ok;
    int blockNumber = item->data(0, Qt::UserRole).toInt(&ok);
    if (ok)
        Q_EMIT headingClicked(blockNumber);
}

void TocWidget::highlightHeading(int sourceLine)
{
    auto it = m_headingsByLine.find(sourceLine);
    QTreeWidgetItem *target = (it != m_headingsByLine.end()) ? *it : nullptr;
    if (m_treeWidget->currentItem() == target)
        return; // already highlighted — no redundant work
    m_treeWidget->blockSignals(true);
    m_treeWidget->setCurrentItem(target);
    if (target)
        m_treeWidget->scrollToItem(target);
    m_treeWidget->blockSignals(false);
}
