// SPDX-License-Identifier: GPL-2.0-or-later

#include "fontpairingpickerwidget.h"

#include <QFont>
#include <QGridLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QToolButton>
#include <QVBoxLayout>

#include "fontpairing.h"
#include "fontpairingmanager.h"

namespace {

// ---------------------------------------------------------------------------
// FontPairingCell — renders three text samples in the respective fonts
// ---------------------------------------------------------------------------
class FontPairingCell : public QWidget
{
    Q_OBJECT

public:
    explicit FontPairingCell(const FontPairing &pairing, bool selected,
                             QWidget *parent = nullptr)
        : QWidget(parent)
        , m_pairing(pairing)
        , m_selected(selected)
    {
        setFixedSize(120, 50);
        setCursor(Qt::PointingHandCursor);
        setToolTip(pairing.name);
    }

    void setSelected(bool selected)
    {
        if (m_selected != selected) {
            m_selected = selected;
            update();
        }
    }

    QString pairingId() const { return m_pairing.id; }

Q_SIGNALS:
    void clicked(const QString &id);

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::TextAntialiasing, true);

        const QRect r = rect();

        // White background
        p.fillRect(r, Qt::white);

        const int textMargin = 4;
        const int fontSize = 9;

        // Body font: render the body family name
        QFont bodyFont(m_pairing.body.family, fontSize);
        p.setFont(bodyFont);
        p.setPen(Qt::black);
        p.drawText(QRect(textMargin, 2, r.width() - 2 * textMargin, 14),
                   Qt::AlignLeft | Qt::AlignVCenter,
                   m_pairing.body.family);

        // Heading font: render the heading family name
        QFont headingFont(m_pairing.heading.family, fontSize);
        headingFont.setBold(true);
        p.setFont(headingFont);
        p.drawText(QRect(textMargin, 17, r.width() - 2 * textMargin, 14),
                   Qt::AlignLeft | Qt::AlignVCenter,
                   m_pairing.heading.family);

        // Mono font: render "mono"
        QFont monoFont(m_pairing.mono.family, fontSize - 1);
        p.setFont(monoFont);
        p.setPen(QColor(100, 100, 100));
        p.drawText(QRect(textMargin, 32, r.width() - 2 * textMargin, 14),
                   Qt::AlignLeft | Qt::AlignVCenter,
                   QStringLiteral("mono"));

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
            Q_EMIT clicked(m_pairing.id);
        QWidget::mousePressEvent(event);
    }

private:
    FontPairing m_pairing;
    bool m_selected = false;
};

} // anonymous namespace

// ---------------------------------------------------------------------------
// FontPairingPickerWidget
// ---------------------------------------------------------------------------

FontPairingPickerWidget::FontPairingPickerWidget(FontPairingManager *manager, QWidget *parent)
    : QWidget(parent)
    , m_manager(manager)
{
    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(4);

    auto *header = new QLabel(QStringLiteral("Font Pairings"), this);
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

    connect(m_manager, &FontPairingManager::pairingsChanged,
            this, &FontPairingPickerWidget::refresh);
}

void FontPairingPickerWidget::setCurrentPairingId(const QString &id)
{
    if (m_currentId == id)
        return;
    m_currentId = id;

    // Update selection state on all cells
    auto cells = findChildren<FontPairingCell *>();
    for (auto *cell : cells)
        cell->setSelected(cell->pairingId() == m_currentId);
}

void FontPairingPickerWidget::refresh()
{
    rebuildGrid();
}

void FontPairingPickerWidget::rebuildGrid()
{
    // Remove all existing items from the grid
    while (QLayoutItem *item = m_gridLayout->takeAt(0)) {
        delete item->widget();
        delete item;
    }

    if (!m_manager)
        return;

    const QStringList ids = m_manager->availablePairings();
    const int columns = 2;

    int row = 0;
    int col = 0;
    for (const QString &id : ids) {
        FontPairing fp = m_manager->pairing(id);
        bool selected = (id == m_currentId);
        auto *cell = new FontPairingCell(fp, selected, this);
        connect(cell, &FontPairingCell::clicked, this, [this](const QString &id) {
            setCurrentPairingId(id);
            Q_EMIT pairingSelected(id);
        });
        m_gridLayout->addWidget(cell, row, col);
        ++col;
        if (col >= columns) {
            col = 0;
            ++row;
        }
    }

    // Add [+] button at the end of the grid
    auto *addButton = new QToolButton(this);
    addButton->setText(QStringLiteral("+"));
    addButton->setFixedSize(120, 50);
    addButton->setCursor(Qt::PointingHandCursor);
    addButton->setToolTip(QStringLiteral("Create new font pairing"));
    connect(addButton, &QToolButton::clicked, this, &FontPairingPickerWidget::createRequested);
    m_gridLayout->addWidget(addButton, row, col);
}

#include "fontpairingpickerwidget.moc"
