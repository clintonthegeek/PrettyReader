#include "styletreemodel.h"
#include "stylemanager.h"
#include "paragraphstyle.h"
#include "characterstyle.h"
#include "tablestyle.h"

#include <QFont>
#include <QColor>
#include <QHash>

StyleTreeModel::StyleTreeModel(QObject *parent)
    : QAbstractItemModel(parent)
    , m_rootNode(new TreeNode)
{
    m_rootNode->name = QStringLiteral("Root");
    m_rootNode->nodeType = TreeNode::Category;
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

    if (m_styleManager)
        buildSemanticTree(m_rootNode);

    endResetModel();
}

// --- Semantic tree building ---

namespace {

struct StyleCategoryEntry {
    const char *styleName;
    const char *categoryName;
};

// Static mapping: style name -> semantic category
static const StyleCategoryEntry s_categoryMap[] = {
    // Body Text
    {"Default Paragraph Style", "Body Text"},
    {"BodyText",                "Body Text"},
    {"BlockQuote",              "Body Text"},
    {"Default Character Style", "Body Text"},
    {"DefaultText",             "Body Text"},
    {"Emphasis",                "Body Text"},
    {"Strong",                  "Body Text"},
    {"StrongEmphasis",          "Body Text"},
    {"Strikethrough",           "Body Text"},
    {"Subscript",               "Body Text"},
    {"Superscript",             "Body Text"},
    // Headings
    {"Heading",                 "Headings"},
    {"Heading1",                "Headings"},
    {"Heading2",                "Headings"},
    {"Heading3",                "Headings"},
    {"Heading4",                "Headings"},
    {"Heading5",                "Headings"},
    {"Heading6",                "Headings"},
    // Code
    {"Code",                    "Code"},
    {"InlineCode",              "Code"},
    {"CodeBlock",               "Code"},
    // Lists
    {"ListItem",                "Lists"},
    {"OrderedListItem",         "Lists"},
    {"UnorderedListItem",       "Lists"},
    {"TaskListItem",            "Lists"},
    // Tables
    {"TableHeader",             "Tables"},
    {"TableBody",               "Tables"},
    // Links
    {"Link",                    "Links"},
    // Math
    {"MathDisplay",             "Math"},
    {"MathInline",              "Math"},
    // Emoji
    {"Emoji",                   "Emoji"},
    // Misc paragraph stubs
    {"HorizontalRule",          "Body Text"},
};

// Category display order
static const char *s_categoryOrder[] = {
    "Body Text", "Headings", "Code", "Lists", "Tables", "Links", "Math", "Emoji", "Document"
};
static const int s_categoryOrderCount = sizeof(s_categoryOrder) / sizeof(s_categoryOrder[0]);

QString categoryForStyle(const QString &styleName)
{
    for (const auto &entry : s_categoryMap) {
        if (styleName == QLatin1String(entry.styleName))
            return QString::fromLatin1(entry.categoryName);
    }
    return QStringLiteral("Other");
}

int categoryOrderIndex(const QString &categoryName)
{
    for (int i = 0; i < s_categoryOrderCount; ++i) {
        if (categoryName == QLatin1String(s_categoryOrder[i]))
            return i;
    }
    return s_categoryOrderCount; // "Other" sorts last
}

} // anonymous namespace

void StyleTreeModel::buildSemanticTree(TreeNode *root)
{
    // Collect all style entries with their types
    struct StyleEntry {
        QString name;
        QString parentName;
        TreeNode::NodeType nodeType;
    };
    QList<StyleEntry> allStyles;

    // Paragraph styles
    for (const QString &name : m_styleManager->paragraphStyleNames()) {
        ParagraphStyle *s = m_styleManager->paragraphStyle(name);
        allStyles.append({name, s ? s->parentStyleName() : QString(),
                          TreeNode::ParagraphStyleNode});
    }

    // Character styles
    for (const QString &name : m_styleManager->characterStyleNames()) {
        CharacterStyle *s = m_styleManager->characterStyle(name);
        allStyles.append({name, s ? s->parentStyleName() : QString(),
                          TreeNode::CharacterStyleNode});
    }

    // Table styles
    for (const QString &name : m_styleManager->tableStyleNames()) {
        allStyles.append({name, QString(), TreeNode::TableStyleNode});
    }

    // Group styles by category
    QHash<QString, QList<int>> categoryStyles; // category name -> indices into allStyles
    for (int i = 0; i < allStyles.size(); ++i) {
        QString cat;
        if (allStyles[i].nodeType == TreeNode::TableStyleNode)
            cat = QStringLiteral("Tables");
        else
            cat = categoryForStyle(allStyles[i].name);
        categoryStyles[cat].append(i);
    }

    // Create category nodes in defined order
    QHash<QString, TreeNode *> categoryNodes;

    auto ensureCategory = [&](const QString &catName) -> TreeNode * {
        auto it = categoryNodes.find(catName);
        if (it != categoryNodes.end())
            return it.value();
        auto *cat = new TreeNode;
        cat->name = catName;
        cat->nodeType = TreeNode::Category;
        cat->parent = root;
        root->children.append(cat);
        categoryNodes[catName] = cat;
        return cat;
    };

    // First pass: create categories in order for those that have styles
    for (int ci = 0; ci < s_categoryOrderCount; ++ci) {
        QString catName = QString::fromLatin1(s_categoryOrder[ci]);
        if (catName == QLatin1String("Document") || categoryStyles.contains(catName))
            ensureCategory(catName);
    }
    // Create "Other" if needed
    if (categoryStyles.contains(QStringLiteral("Other")))
        ensureCategory(QStringLiteral("Other"));

    // Second pass: build within-category parent-child trees
    for (auto catIt = categoryStyles.begin(); catIt != categoryStyles.end(); ++catIt) {
        const QString &catName = catIt.key();
        const QList<int> &indices = catIt.value();
        TreeNode *catNode = ensureCategory(catName);

        // Create nodes for all styles in this category
        QHash<QString, TreeNode *> nodeMap;
        for (int idx : indices) {
            const auto &entry = allStyles[idx];
            auto *node = new TreeNode;
            node->name = entry.name;
            node->styleName = entry.name;
            node->nodeType = entry.nodeType;
            nodeMap[entry.name] = node;
        }

        // Build parent-child within the same category
        for (int idx : indices) {
            const auto &entry = allStyles[idx];
            TreeNode *node = nodeMap[entry.name];
            QString parentName = entry.parentName;

            if (!parentName.isEmpty() && nodeMap.contains(parentName)) {
                // Parent is in the same category
                node->parent = nodeMap[parentName];
                nodeMap[parentName]->children.append(node);
            } else {
                // Top-level in this category
                node->parent = catNode;
                catNode->children.append(node);
            }
        }
    }

    // Append Document category with Footnotes
    TreeNode *docCat = ensureCategory(QStringLiteral("Document"));
    auto *fnNode = new TreeNode;
    fnNode->name = tr("Footnotes");
    fnNode->nodeType = TreeNode::FootnoteNode;
    fnNode->parent = docCat;
    docCat->children.append(fnNode);
}

bool StyleTreeModel::isFootnoteNode(const QModelIndex &index) const
{
    if (!index.isValid())
        return false;
    auto *node = static_cast<TreeNode *>(index.internalPointer());
    return node->nodeType == TreeNode::FootnoteNode;
}

bool StyleTreeModel::isTableStyle(const QModelIndex &index) const
{
    if (!index.isValid())
        return false;
    auto *node = static_cast<TreeNode *>(index.internalPointer());
    return node->nodeType == TreeNode::TableStyleNode;
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
    return node->nodeType == TreeNode::ParagraphStyleNode;
}

bool StyleTreeModel::isCharacterStyle(const QModelIndex &index) const
{
    if (!index.isValid())
        return false;
    auto *node = static_cast<TreeNode *>(index.internalPointer());
    return node->nodeType == TreeNode::CharacterStyleNode;
}

bool StyleTreeModel::isCategoryNode(const QModelIndex &index) const
{
    if (!index.isValid())
        return false;
    auto *node = static_cast<TreeNode *>(index.internalPointer());
    return node->nodeType == TreeNode::Category;
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

    if (m_showPreviews && node->nodeType != TreeNode::Category
        && !node->styleName.isEmpty() && m_styleManager) {
        if (node->nodeType == TreeNode::ParagraphStyleNode) {
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
        } else if (node->nodeType == TreeNode::CharacterStyleNode) {
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
