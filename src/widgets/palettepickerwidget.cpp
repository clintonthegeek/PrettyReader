// SPDX-License-Identifier: GPL-2.0-or-later

#include "palettepickerwidget.h"

#include <QPainter>

#include "colorpalette.h"
#include "palettemanager.h"

namespace {

class PaletteSwatchCell : public ResourcePickerCellBase
{
    Q_OBJECT

public:
    explicit PaletteSwatchCell(const ColorPalette &palette, bool selected,
                               QWidget *parent = nullptr)
        : ResourcePickerCellBase(palette.id, selected, parent)
        , m_palette(palette)
    {
        setFixedSize(75, 52);
        setToolTip(palette.name);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, false);

        const QRect r = rect();

        // Fill background with pageBackground
        p.fillRect(r, m_palette.pageBackground());

        // Draw color strips in the upper portion of the cell
        const int stripHeight = 4;
        const int stripMargin = 4;
        const int stripWidth = r.width() - 2 * stripMargin;
        int y = 4;

        p.fillRect(QRect(stripMargin, y, stripWidth, stripHeight), m_palette.text());
        y += stripHeight + 2;

        p.fillRect(QRect(stripMargin, y, stripWidth, stripHeight), m_palette.headingText());
        y += stripHeight + 2;

        p.fillRect(QRect(stripMargin, y, stripWidth / 2, stripHeight), m_palette.linkText());
        p.fillRect(QRect(stripMargin + stripWidth / 2 + 2, y, stripWidth / 2 - 2, stripHeight),
                   m_palette.surfaceCode());

        // Name label at the bottom
        QFont nameFont = font();
        nameFont.setPointSize(6);
        p.setFont(nameFont);
        p.setPen(m_palette.text());
        QRect nameRect(stripMargin, 30, stripWidth, 18);
        p.drawText(nameRect, Qt::AlignLeft | Qt::AlignVCenter,
                   p.fontMetrics().elidedText(m_palette.name, Qt::ElideRight, stripWidth));

        drawSelectionBorder(p);
    }

private:
    ColorPalette m_palette;
};

} // anonymous namespace

PalettePickerWidget::PalettePickerWidget(PaletteManager *manager, QWidget *parent)
    : ResourcePickerWidget(QStringLiteral("Color Palettes"), parent)
    , m_manager(manager)
{
    rebuildGrid();
    connect(m_manager, &PaletteManager::palettesChanged, this, &PalettePickerWidget::refresh);
}

void PalettePickerWidget::populateGrid()
{
    if (!m_manager)
        return;

    const QStringList ids = m_manager->availablePalettes();
    for (const QString &id : ids) {
        ColorPalette pal = m_manager->palette(id);
        addCell(new PaletteSwatchCell(pal, id == m_currentId, this));
    }
}

#include "palettepickerwidget.moc"
