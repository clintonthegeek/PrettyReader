#include "sidebar.h"
#include "toolview.h"

#include <KMultiTabBar>
#include <QHBoxLayout>
#include <QSplitter>
#include <QStackedWidget>

Sidebar::Sidebar(Position position, QWidget *parent)
    : QWidget(parent)
    , m_position(position)
{
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    KMultiTabBar::KMultiTabBarPosition barPos =
        (position == Left) ? KMultiTabBar::Left : KMultiTabBar::Right;

    m_tabBar = new KMultiTabBar(barPos, this);
    m_tabBar->setStyle(KMultiTabBar::VSNET);

    m_stack = new QStackedWidget(this);
    m_stack->setVisible(false);

    if (position == Left) {
        layout->addWidget(m_tabBar);
        layout->addWidget(m_stack, 1);
    } else {
        layout->addWidget(m_stack, 1);
        layout->addWidget(m_tabBar);
    }
}

int Sidebar::addPanel(ToolView *view, const QIcon &icon, const QString &tooltip)
{
    int tabId = m_nextTabId++;

    m_tabBar->appendTab(icon, tabId, tooltip);
    connect(m_tabBar->tab(tabId), &KMultiTabBarTab::clicked,
            this, [this, tabId]() { onTabClicked(tabId); });

    m_stack->addWidget(view);
    m_panels.append({tabId, view});

    connect(view, &ToolView::closeRequested,
            this, [this, tabId]() { hidePanel(tabId); });

    return tabId;
}

void Sidebar::showPanel(int tabId)
{
    for (const auto &p : m_panels) {
        if (p.tabId == tabId) {
            m_stack->setCurrentWidget(p.view);
            m_activeTab = tabId;
            m_collapsed = false;
            updateVisibility();

            // Update tab button states
            for (const auto &pp : m_panels)
                m_tabBar->setTab(pp.tabId, pp.tabId == tabId);

            Q_EMIT panelVisibilityChanged(tabId, true);
            return;
        }
    }
}

void Sidebar::hidePanel(int tabId)
{
    if (m_activeTab == tabId) {
        m_activeTab = -1;
        m_collapsed = true;
        updateVisibility();
        m_tabBar->setTab(tabId, false);
        Q_EMIT panelVisibilityChanged(tabId, false);
    }
}

void Sidebar::togglePanel(int tabId)
{
    if (m_activeTab == tabId)
        hidePanel(tabId);
    else
        showPanel(tabId);
}

bool Sidebar::isPanelVisible(int tabId) const
{
    return m_activeTab == tabId && !m_collapsed;
}

void Sidebar::setCollapsed(bool collapsed)
{
    m_collapsed = collapsed;
    if (collapsed) {
        m_activeTab = -1;
        for (const auto &p : m_panels)
            m_tabBar->setTab(p.tabId, false);
    }
    updateVisibility();
}

void Sidebar::onTabClicked(int tabId)
{
    togglePanel(tabId);
}

int Sidebar::tabBarWidth() const
{
    return qMax(m_tabBar->sizeHint().width(), 24);
}

void Sidebar::updateVisibility()
{
    m_stack->setVisible(!m_collapsed);

    int barW = tabBarWidth();

    if (m_collapsed) {
        // Remember expanded width for later restoration
        if (width() > barW + 20)
            m_expandedWidth = width();

        // Lock sidebar to just the tab bar strip
        setFixedWidth(barW);

        // Explicitly shrink in the splitter (needed for right-side sidebars
        // where QSplitter doesn't automatically reclaim the freed space)
        auto *splitter = qobject_cast<QSplitter *>(parentWidget());
        if (splitter) {
            int idx = splitter->indexOf(this);
            if (idx >= 0) {
                QList<int> sizes = splitter->sizes();
                int freed = sizes[idx] - barW;
                if (freed > 0) {
                    sizes[idx] = barW;
                    sizes[1] += freed;
                    splitter->setSizes(sizes);
                }
            }
        }
    } else {
        // Unlock width constraints
        setMinimumWidth(barW);
        setMaximumWidth(QWIDGETSIZE_MAX);

        // Ask parent splitter to allocate space
        auto *splitter = qobject_cast<QSplitter *>(parentWidget());
        if (splitter) {
            int idx = splitter->indexOf(this);
            if (idx >= 0) {
                QList<int> sizes = splitter->sizes();
                int target = qMax(m_expandedWidth, 250);
                int diff = target - sizes[idx];
                if (diff > 0 && sizes.size() > 1) {
                    sizes[idx] = target;
                    // Take space from center widget (index 1)
                    sizes[1] = qMax(200, sizes[1] - diff);
                    splitter->setSizes(sizes);
                }
            }
        }
    }
}
