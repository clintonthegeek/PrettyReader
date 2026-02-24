// SPDX-License-Identifier: GPL-2.0-or-later

#include "pagetemplatepickerwidget.h"

#include <QGridLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QVBoxLayout>

#include "pagetemplate.h"
#include "pagetemplatemanager.h"

namespace {

// ---------------------------------------------------------------------------
// PageTemplateCell — renders a mini page outline with template name
// ---------------------------------------------------------------------------
class PageTemplateCell : public QWidget
{
    Q_OBJECT

public:
    explicit PageTemplateCell(const PageTemplate &tmpl, bool selected,
                              QWidget *parent = nullptr)
        : QWidget(parent)
        , m_template(tmpl)
        , m_selected(selected)
    {
        setFixedSize(120, 50);
        setCursor(Qt::PointingHandCursor);
        setToolTip(tmpl.description.isEmpty() ? tmpl.name : tmpl.description);
    }

    void setSelected(bool selected)
    {
        if (m_selected != selected) {
            m_selected = selected;
            update();
        }
    }

    QString templateId() const { return m_template.id; }

Q_SIGNALS:
    void clicked(const QString &id);

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        const QRect r = rect();
        p.fillRect(r, Qt::white);

        // Draw a mini page outline (centered, proportional)
        const int pageW = 28;
        const int pageH = 36;
        const int pageX = 6;
        const int pageY = (r.height() - pageH) / 2;

        p.setPen(QPen(QColor(160, 160, 160), 1));
        p.setBrush(QColor(250, 250, 250));
        p.drawRect(pageX, pageY, pageW, pageH);

        // Draw margin lines
        const auto &pl = m_template.pageLayout;
        const int marginPx = 3; // representative margin inset
        p.setPen(QPen(QColor(200, 200, 220), 0.5, Qt::DotLine));
        p.drawRect(pageX + marginPx, pageY + marginPx,
                   pageW - 2 * marginPx, pageH - 2 * marginPx);

        // Draw header/footer indicators
        if (pl.headerEnabled) {
            p.setPen(QPen(QColor(120, 160, 200), 1));
            int hy = pageY + marginPx + 2;
            p.drawLine(pageX + marginPx + 1, hy, pageX + pageW - marginPx - 1, hy);
        }
        if (pl.footerEnabled) {
            p.setPen(QPen(QColor(120, 160, 200), 1));
            int fy = pageY + pageH - marginPx - 2;
            p.drawLine(pageX + marginPx + 1, fy, pageX + pageW - marginPx - 1, fy);
        }

        // Draw template name to the right of the page icon
        p.setPen(Qt::black);
        QFont nameFont = font();
        nameFont.setPointSize(8);
        p.setFont(nameFont);
        QRect textRect(pageX + pageW + 6, 4, r.width() - pageX - pageW - 12, 16);
        p.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, m_template.name);

        // Draw page size below name
        QPageSize ps(pl.pageSizeId);
        QString sizeLabel = ps.name();
        p.setPen(QColor(100, 100, 100));
        QFont detailFont = font();
        detailFont.setPointSize(7);
        p.setFont(detailFont);
        QRect detailRect(pageX + pageW + 6, 20, r.width() - pageX - pageW - 12, 14);
        p.drawText(detailRect, Qt::AlignLeft | Qt::AlignVCenter, sizeLabel);

        // Draw header/footer labels
        QString hfLabel;
        if (pl.headerEnabled && pl.footerEnabled)
            hfLabel = QStringLiteral("H+F");
        else if (pl.headerEnabled)
            hfLabel = QStringLiteral("H");
        else if (pl.footerEnabled)
            hfLabel = QStringLiteral("F");
        if (!hfLabel.isEmpty()) {
            QRect hfRect(pageX + pageW + 6, 34, r.width() - pageX - pageW - 12, 12);
            p.drawText(hfRect, Qt::AlignLeft | Qt::AlignVCenter, hfLabel);
        }

        // Draw selection border
        if (m_selected) {
            QPen pen(palette().color(QPalette::Highlight), 2);
            p.setPen(pen);
            p.drawRect(r.adjusted(1, 1, -1, -1));
        } else {
            QPen pen(palette().color(QPalette::Mid), 1);
            p.setPen(pen);
            p.drawRect(r.adjusted(0, 0, -1, -1));
        }
    }

    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton)
            Q_EMIT clicked(m_template.id);
        QWidget::mousePressEvent(event);
    }

private:
    PageTemplate m_template;
    bool m_selected = false;
};

} // anonymous namespace

// ---------------------------------------------------------------------------
// PageTemplatePickerWidget
// ---------------------------------------------------------------------------

PageTemplatePickerWidget::PageTemplatePickerWidget(PageTemplateManager *manager, QWidget *parent)
    : QWidget(parent)
    , m_manager(manager)
{
    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(4);

    auto *header = new QLabel(QStringLiteral("Page Templates"), this);
    QFont headerFont = header->font();
    headerFont.setBold(true);
    header->setFont(headerFont);
    outerLayout->addWidget(header);

    // Container widget for the grid
    auto *gridContainer = new QWidget(this);
    m_gridLayout = new QGridLayout(gridContainer);
    m_gridLayout->setContentsMargins(0, 0, 0, 0);
    m_gridLayout->setSpacing(4);
    outerLayout->addWidget(gridContainer);

    outerLayout->addStretch();

    rebuildGrid();

    connect(m_manager, &PageTemplateManager::templatesChanged,
            this, &PageTemplatePickerWidget::refresh);
}

void PageTemplatePickerWidget::setCurrentTemplateId(const QString &id)
{
    if (m_currentId == id)
        return;
    m_currentId = id;

    auto cells = findChildren<PageTemplateCell *>();
    for (auto *cell : cells)
        cell->setSelected(cell->templateId() == m_currentId);
}

void PageTemplatePickerWidget::refresh()
{
    rebuildGrid();
}

void PageTemplatePickerWidget::rebuildGrid()
{
    while (QLayoutItem *item = m_gridLayout->takeAt(0)) {
        delete item->widget();
        delete item;
    }

    if (!m_manager)
        return;

    const QStringList ids = m_manager->availableTemplates();
    const int columns = 2;

    int row = 0;
    int col = 0;
    for (const QString &id : ids) {
        PageTemplate tmpl = m_manager->pageTemplate(id);
        bool selected = (id == m_currentId);
        auto *cell = new PageTemplateCell(tmpl, selected, this);
        connect(cell, &PageTemplateCell::clicked, this, [this](const QString &clickedId) {
            setCurrentTemplateId(clickedId);
            Q_EMIT templateSelected(clickedId);
        });
        m_gridLayout->addWidget(cell, row, col);
        ++col;
        if (col >= columns) {
            col = 0;
            ++row;
        }
    }

}

#include "pagetemplatepickerwidget.moc"
