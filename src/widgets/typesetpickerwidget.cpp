// SPDX-License-Identifier: GPL-2.0-or-later

#include "typesetpickerwidget.h"

#include <QFont>
#include <QGridLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QToolButton>
#include <QVBoxLayout>

#include "typeset.h"
#include "typesetmanager.h"

namespace {

// ---------------------------------------------------------------------------
// TypeSetCell — renders three text samples in the respective fonts
// ---------------------------------------------------------------------------
class TypeSetCell : public QWidget
{
    Q_OBJECT

public:
    explicit TypeSetCell(const TypeSet &typeSet, bool selected,
                         QWidget *parent = nullptr)
        : QWidget(parent)
        , m_typeSet(typeSet)
        , m_selected(selected)
    {
        setFixedSize(120, 62);
        setCursor(Qt::PointingHandCursor);
        setToolTip(typeSet.name);
    }

    void setSelected(bool selected)
    {
        if (m_selected != selected) {
            m_selected = selected;
            update();
        }
    }

    QString typeSetId() const { return m_typeSet.id; }

Q_SIGNALS:
    void clicked(const QString &id);

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::TextAntialiasing, true);

        const QRect r = rect();
        p.fillRect(r, Qt::white);

        const int textMargin = 4;

        // Name label at top
        QFont nameFont = font();
        nameFont.setPointSize(7);
        nameFont.setBold(true);
        p.setFont(nameFont);
        p.setPen(Qt::black);
        p.drawText(QRect(textMargin, 2, r.width() - 2 * textMargin, 12),
                   Qt::AlignLeft | Qt::AlignVCenter, m_typeSet.name);

        // Body font sample
        const int fontSize = 8;
        const int sampleTop = 15;
        QFont bodyFont(m_typeSet.body.family, fontSize);
        p.setFont(bodyFont);
        p.setPen(Qt::black);
        p.drawText(QRect(textMargin, sampleTop, r.width() - 2 * textMargin, 13),
                   Qt::AlignLeft | Qt::AlignVCenter,
                   m_typeSet.body.family);

        // Heading font sample (bold)
        QFont headingFont(m_typeSet.heading.family, fontSize);
        headingFont.setBold(true);
        p.setFont(headingFont);
        p.drawText(QRect(textMargin, sampleTop + 14, r.width() - 2 * textMargin, 13),
                   Qt::AlignLeft | Qt::AlignVCenter,
                   m_typeSet.heading.family);

        // Mono font sample
        QFont monoFont(m_typeSet.mono.family, fontSize - 1);
        p.setFont(monoFont);
        p.setPen(QColor(100, 100, 100));
        p.drawText(QRect(textMargin, sampleTop + 28, r.width() - 2 * textMargin, 13),
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
            Q_EMIT clicked(m_typeSet.id);
        QWidget::mousePressEvent(event);
    }

private:
    TypeSet m_typeSet;
    bool m_selected = false;
};

} // anonymous namespace

// ---------------------------------------------------------------------------
// TypeSetPickerWidget
// ---------------------------------------------------------------------------

TypeSetPickerWidget::TypeSetPickerWidget(TypeSetManager *manager, QWidget *parent)
    : QWidget(parent)
    , m_manager(manager)
{
    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(4);

    auto *header = new QLabel(QStringLiteral("Type Sets"), this);
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

    connect(m_manager, &TypeSetManager::typeSetsChanged,
            this, &TypeSetPickerWidget::refresh);
}

void TypeSetPickerWidget::setCurrentTypeSetId(const QString &id)
{
    if (m_currentId == id)
        return;
    m_currentId = id;

    auto cells = findChildren<TypeSetCell *>();
    for (auto *cell : cells)
        cell->setSelected(cell->typeSetId() == m_currentId);
}

void TypeSetPickerWidget::refresh()
{
    rebuildGrid();
}

void TypeSetPickerWidget::rebuildGrid()
{
    while (QLayoutItem *item = m_gridLayout->takeAt(0)) {
        delete item->widget();
        delete item;
    }

    if (!m_manager)
        return;

    const QStringList ids = m_manager->availableTypeSets();
    const int columns = 2;

    int row = 0;
    int col = 0;
    for (const QString &id : ids) {
        TypeSet ts = m_manager->typeSet(id);
        bool selected = (id == m_currentId);
        auto *cell = new TypeSetCell(ts, selected, this);
        connect(cell, &TypeSetCell::clicked, this, [this](const QString &clickedId) {
            setCurrentTypeSetId(clickedId);
            Q_EMIT typeSetSelected(clickedId);
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
    addButton->setFixedSize(120, 62);
    addButton->setCursor(Qt::PointingHandCursor);
    addButton->setToolTip(QStringLiteral("Create new type set"));
    connect(addButton, &QToolButton::clicked, this, &TypeSetPickerWidget::createRequested);
    m_gridLayout->addWidget(addButton, row, col);
}

#include "typesetpickerwidget.moc"
