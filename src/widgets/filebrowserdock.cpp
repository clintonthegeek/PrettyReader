#include "filebrowserdock.h"

#include <KDirLister>
#include <KDirModel>
#include <KDirSortFilterProxyModel>
#include <KFileItem>

#include <QDir>
#include <QLineEdit>
#include <QTreeView>
#include <QVBoxLayout>

FileBrowserDock::FileBrowserDock(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    // Path edit bar
    m_pathEdit = new QLineEdit(this);
    m_pathEdit->setPlaceholderText(tr("Directory path..."));
    m_pathEdit->setClearButtonEnabled(true);
    layout->addWidget(m_pathEdit);

    // Dir model
    m_dirModel = new KDirModel(this);

    // Filter proxy - show only directories and markdown files
    m_proxyModel = new KDirSortFilterProxyModel(this);
    m_proxyModel->setSourceModel(m_dirModel);
    m_proxyModel->setSortFoldersFirst(true);

    // Tree view
    m_treeView = new QTreeView(this);
    m_treeView->setModel(m_proxyModel);
    m_treeView->setHeaderHidden(true);
    // Only show the Name column
    for (int i = 1; i < m_dirModel->columnCount(); ++i)
        m_treeView->setColumnHidden(i, true);
    m_treeView->setRootIsDecorated(true);
    layout->addWidget(m_treeView);

    connect(m_treeView, &QTreeView::activated,
            this, &FileBrowserDock::onItemActivated);
    connect(m_pathEdit, &QLineEdit::returnPressed,
            this, &FileBrowserDock::onPathEdited);

    // Default to home directory
    setRootPath(QDir::homePath());
}

void FileBrowserDock::setRootPath(const QString &path)
{
    QUrl url = QUrl::fromLocalFile(path);
    m_dirModel->dirLister()->openUrl(url);
    m_pathEdit->setText(path);
}

QString FileBrowserDock::rootPath() const
{
    return m_pathEdit->text();
}

void FileBrowserDock::onItemActivated(const QModelIndex &index)
{
    QModelIndex sourceIndex = m_proxyModel->mapToSource(index);
    KFileItem item = m_dirModel->itemForIndex(sourceIndex);
    if (item.isNull())
        return;

    if (item.isDir()) {
        setRootPath(item.localPath());
    } else {
        // Check if it's a markdown file
        QString name = item.name().toLower();
        if (name.endsWith(QLatin1String(".md"))
            || name.endsWith(QLatin1String(".markdown"))
            || name.endsWith(QLatin1String(".mkd"))
            || name.endsWith(QLatin1String(".txt"))) {
            Q_EMIT fileActivated(item.url());
        }
    }
}

void FileBrowserDock::onPathEdited()
{
    QString path = m_pathEdit->text().trimmed();
    if (!path.isEmpty() && QDir(path).exists()) {
        setRootPath(path);
    }
}
