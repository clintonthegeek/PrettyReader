// SPDX-License-Identifier: GPL-2.0-or-later

#include "pagetemplatepickerwidget.h"

#include <QPageSize>
#include <QPainter>

#include "pagetemplate.h"
#include "pagetemplatemanager.h"

namespace {

class PageTemplateCell : public ResourcePickerCellBase
{
    Q_OBJECT

public:
    explicit PageTemplateCell(const PageTemplate &tmpl, bool selected,
                              QWidget *parent = nullptr)
        : ResourcePickerCellBase(tmpl.id, selected, parent)
        , m_template(tmpl)
    {
        setFixedSize(120, 50);
        setToolTip(tmpl.description.isEmpty() ? tmpl.name : tmpl.description);
    }

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
        const int marginPx = 3;
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

        drawSelectionBorder(p);
    }

private:
    PageTemplate m_template;
};

} // anonymous namespace

PageTemplatePickerWidget::PageTemplatePickerWidget(PageTemplateManager *manager, QWidget *parent)
    : ResourcePickerWidget(QStringLiteral("Page Templates"), parent)
    , m_manager(manager)
{
    rebuildGrid();
    connect(m_manager, &PageTemplateManager::templatesChanged,
            this, &PageTemplatePickerWidget::refresh);
}

void PageTemplatePickerWidget::populateGrid()
{
    if (!m_manager)
        return;

    const QStringList ids = m_manager->availableTemplates();
    for (const QString &id : ids) {
        PageTemplate tmpl = m_manager->pageTemplate(id);
        addCell(new PageTemplateCell(tmpl, id == m_currentId, this));
    }
}

#include "pagetemplatepickerwidget.moc"
