#include "styletreemodel.h"
#include "stylemanager.h"
#include "paragraphstyle.h"
#include "characterstyle.h"

#include <QFont>
#include <QColor>

StyleTreeModel::StyleTreeModel(QObject *parent)
    : QAbstractItemModel(parent)
    , m_rootNode(new TreeNode)
{
    m_rootNode->name = QStringLiteral("Root");
    m_rootNode->isCategory = true;
}

StyleTreeModel::~StyleTreeModel()
{
    delete m_rootNode;
}

void StyleTreeModel::setStyleManager(StyleManager *sm)
{
    m_styleManager = sm;
    rebuildTree();
}

void StyleTreeModel::setShowPreviews(bool show)
{
    if (m_showPreviews == show)
        return;
    m_showPreviews = show;
    if (m_rootNode->children.isEmpty())
        return;
    Q_EMIT dataChanged(index(0, 0), index(rowCount() - 1, 0));
}

void StyleTreeModel::refresh()
{
    rebuildTree();
}

void StyleTreeModel::rebuildTree()
{
    beginResetModel();
    qDeleteAll(m_rootNode->children);
    m_rootNode->children.clear();

    if (m_styleManager) {
        addParagraphSubtree(m_rootNode);
        addCharacterSubtree(m_rootNode);
    }

    endResetModel();
}

void StyleTreeModel::addParagraphSubtree(TreeNode *root)
{
    auto *cat = new TreeNode;
    cat->name = tr("Paragraph Styles");
    cat->isCategory = true;
    cat->isParagraph = true;
    cat->parent = root;
    root->children.append(cat);

    // Build tree from parent-child relationships
    // First, collect all style names and create nodes
    QHash<QString, TreeNode *> nodeMap;
    QStringList names = m_styleManager->paragraphStyleNames();

    for (const QString &name : names) {
        auto *node = new TreeNode;
        node->name = name;
        node->styleName = name;
        node->isParagraph = true;
        nodeMap[name] = node;
    }

    // Assign parent-child relationships
    for (const QString &name : names) {
        ParagraphStyle *style = m_styleManager->paragraphStyle(name);
        if (!style) continue;

        TreeNode *node = nodeMap[name];
        QString parentName = style->parentStyleName();

        if (!parentName.isEmpty() && nodeMap.contains(parentName)) {
            node->parent = nodeMap[parentName];
            nodeMap[parentName]->children.append(node);
        } else {
            // Top-level style under category
            node->parent = cat;
            cat->children.append(node);
        }
    }
}

void StyleTreeModel::addCharacterSubtree(TreeNode *root)
{
    auto *cat = new TreeNode;
    cat->name = tr("Character Styles");
    cat->isCategory = true;
    cat->isParagraph = false;
    cat->parent = root;
    root->children.append(cat);

    QHash<QString, TreeNode *> nodeMap;
    QStringList names = m_styleManager->characterStyleNames();

    for (const QString &name : names) {
        auto *node = new TreeNode;
        node->name = name;
        node->styleName = name;
        node->isParagraph = false;
        nodeMap[name] = node;
    }

    for (const QString &name : names) {
        CharacterStyle *style = m_styleManager->characterStyle(name);
        if (!style) continue;

        TreeNode *node = nodeMap[name];
        QString parentName = style->parentStyleName();

        if (!parentName.isEmpty() && nodeMap.contains(parentName)) {
            node->parent = nodeMap[parentName];
            nodeMap[parentName]->children.append(node);
        } else {
            node->parent = cat;
            cat->children.append(node);
        }
    }
}

StyleTreeModel::TreeNode *StyleTreeModel::findChildByStyleName(
    TreeNode *parent, const QString &name) const
{
    for (auto *child : parent->children) {
        if (child->styleName == name)
            return child;
        auto *found = findChildByStyleName(child, name);
        if (found)
            return found;
    }
    return nullptr;
}

QString StyleTreeModel::styleName(const QModelIndex &index) const
{
    if (!index.isValid())
        return {};
    auto *node = static_cast<TreeNode *>(index.internalPointer());
    return node->styleName;
}

bool StyleTreeModel::isParagraphStyle(const QModelIndex &index) const
{
    if (!index.isValid())
        return false;
    auto *node = static_cast<TreeNode *>(index.internalPointer());
    return node->isParagraph && !node->isCategory;
}

bool StyleTreeModel::isCharacterStyle(const QModelIndex &index) const
{
    if (!index.isValid())
        return false;
    auto *node = static_cast<TreeNode *>(index.internalPointer());
    return !node->isParagraph && !node->isCategory;
}

bool StyleTreeModel::isCategoryNode(const QModelIndex &index) const
{
    if (!index.isValid())
        return false;
    auto *node = static_cast<TreeNode *>(index.internalPointer());
    return node->isCategory;
}

QModelIndex StyleTreeModel::index(int row, int column, const QModelIndex &parent) const
{
    if (column != 0)
        return {};

    TreeNode *parentNode;
    if (!parent.isValid())
        parentNode = m_rootNode;
    else
        parentNode = static_cast<TreeNode *>(parent.internalPointer());

    if (row < 0 || row >= parentNode->children.size())
        return {};

    return createIndex(row, column, parentNode->children.at(row));
}

QModelIndex StyleTreeModel::parent(const QModelIndex &child) const
{
    if (!child.isValid())
        return {};

    auto *childNode = static_cast<TreeNode *>(child.internalPointer());
    TreeNode *parentNode = childNode->parent;

    if (!parentNode || parentNode == m_rootNode)
        return {};

    // Find the row of parentNode in its parent
    TreeNode *grandParent = parentNode->parent;
    if (!grandParent)
        return {};

    int row = grandParent->children.indexOf(parentNode);
    return createIndex(row, 0, parentNode);
}

int StyleTreeModel::rowCount(const QModelIndex &parent) const
{
    TreeNode *parentNode;
    if (!parent.isValid())
        parentNode = m_rootNode;
    else
        parentNode = static_cast<TreeNode *>(parent.internalPointer());

    return parentNode->children.size();
}

int StyleTreeModel::columnCount(const QModelIndex &) const
{
    return 1;
}

QVariant StyleTreeModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return {};

    auto *node = static_cast<TreeNode *>(index.internalPointer());

    if (role == Qt::DisplayRole)
        return node->name;

    if (m_showPreviews && !node->isCategory && !node->styleName.isEmpty() && m_styleManager) {
        if (node->isParagraph) {
            ParagraphStyle style = m_styleManager->resolvedParagraphStyle(node->styleName);
            if (role == Qt::FontRole) {
                QFont f(style.fontFamily());
                f.setPointSizeF(qBound(8.0, style.fontSize(), 14.0));
                f.setWeight(style.fontWeight());
                f.setItalic(style.fontItalic());
                return f;
            }
            if (role == Qt::ForegroundRole && style.hasForeground())
                return style.foreground();
        } else {
            CharacterStyle style = m_styleManager->resolvedCharacterStyle(node->styleName);
            if (role == Qt::FontRole) {
                QFont f(style.fontFamily());
                f.setPointSizeF(qBound(8.0, style.fontSize(), 14.0));
                f.setWeight(style.fontWeight());
                f.setItalic(style.fontItalic());
                f.setUnderline(style.fontUnderline());
                f.setStrikeOut(style.fontStrikeOut());
                return f;
            }
            if (role == Qt::ForegroundRole && style.hasForeground())
                return style.foreground();
        }
    }

    return {};
}

Qt::ItemFlags StyleTreeModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}
