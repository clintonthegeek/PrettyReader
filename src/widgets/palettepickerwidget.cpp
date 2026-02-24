// SPDX-License-Identifier: GPL-2.0-or-later

#include "palettepickerwidget.h"

#include <QGridLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QVBoxLayout>

#include "colorpalette.h"
#include "palettemanager.h"

namespace {

// ---------------------------------------------------------------------------
// PaletteSwatchCell — a small cell that paints representative palette colors
// ---------------------------------------------------------------------------
class PaletteSwatchCell : public QWidget
{
    Q_OBJECT

public:
    explicit PaletteSwatchCell(const ColorPalette &palette, bool selected,
                               QWidget *parent = nullptr)
        : QWidget(parent)
        , m_palette(palette)
        , m_selected(selected)
    {
        setFixedSize(75, 52);
        setCursor(Qt::PointingHandCursor);
        setToolTip(palette.name);
    }

    void setSelected(bool selected)
    {
        if (m_selected != selected) {
            m_selected = selected;
            update();
        }
    }

    QString paletteId() const { return m_palette.id; }

Q_SIGNALS:
    void clicked(const QString &id);

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

        // Text color strip
        p.fillRect(QRect(stripMargin, y, stripWidth, stripHeight), m_palette.text());
        y += stripHeight + 2;

        // Heading text strip
        p.fillRect(QRect(stripMargin, y, stripWidth, stripHeight), m_palette.headingText());
        y += stripHeight + 2;

        // Link text strip (half width)
        p.fillRect(QRect(stripMargin, y, stripWidth / 2, stripHeight), m_palette.linkText());

        // Code surface strip (other half)
        p.fillRect(QRect(stripMargin + stripWidth / 2 + 2, y, stripWidth / 2 - 2, stripHeight),
                   m_palette.surfaceCode());

        // Name label at the bottom
        QFont nameFont = font();
        nameFont.setPointSize(6);
        p.setFont(nameFont);
        // Use text color from the palette itself for contrast against its background
        p.setPen(m_palette.text());
        QRect nameRect(stripMargin, 30, stripWidth, 18);
        p.drawText(nameRect, Qt::AlignLeft | Qt::AlignVCenter,
                   p.fontMetrics().elidedText(m_palette.name, Qt::ElideRight, stripWidth));

        // Draw border
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
            Q_EMIT clicked(m_palette.id);
        QWidget::mousePressEvent(event);
    }

private:
    ColorPalette m_palette;
    bool m_selected = false;
};

} // anonymous namespace

// ---------------------------------------------------------------------------
// PalettePickerWidget
// ---------------------------------------------------------------------------

PalettePickerWidget::PalettePickerWidget(PaletteManager *manager, QWidget *parent)
    : QWidget(parent)
    , m_manager(manager)
{
    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(4);

    auto *header = new QLabel(QStringLiteral("Color Palettes"), this);
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

    connect(m_manager, &PaletteManager::palettesChanged, this, &PalettePickerWidget::refresh);
}

void PalettePickerWidget::setCurrentPaletteId(const QString &id)
{
    if (m_currentId == id)
        return;
    m_currentId = id;

    // Update selection state on all swatch cells
    auto cells = findChildren<PaletteSwatchCell *>();
    for (auto *cell : cells)
        cell->setSelected(cell->paletteId() == m_currentId);
}

void PalettePickerWidget::refresh()
{
    rebuildGrid();
}

void PalettePickerWidget::rebuildGrid()
{
    // Remove all existing items from the grid
    while (QLayoutItem *item = m_gridLayout->takeAt(0)) {
        delete item->widget();
        delete item;
    }

    if (!m_manager)
        return;

    const QStringList ids = m_manager->availablePalettes();
    const int columns = 3;

    int row = 0;
    int col = 0;
    for (const QString &id : ids) {
        ColorPalette pal = m_manager->palette(id);
        bool selected = (id == m_currentId);
        auto *cell = new PaletteSwatchCell(pal, selected, this);
        connect(cell, &PaletteSwatchCell::clicked, this, [this](const QString &id) {
            setCurrentPaletteId(id);
            Q_EMIT paletteSelected(id);
        });
        m_gridLayout->addWidget(cell, row, col);
        ++col;
        if (col >= columns) {
            col = 0;
            ++row;
        }
    }

}

#include "palettepickerwidget.moc"
