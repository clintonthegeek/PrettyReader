// SPDX-License-Identifier: GPL-2.0-or-later

#include "typographythemepickerwidget.h"

#include <QFont>
#include <QGridLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QToolButton>
#include <QVBoxLayout>

#include "typographytheme.h"
#include "typographythememanager.h"

namespace {

// ---------------------------------------------------------------------------
// TypographyThemeCell — renders three text samples in the respective fonts
// ---------------------------------------------------------------------------
class TypographyThemeCell : public QWidget
{
    Q_OBJECT

public:
    explicit TypographyThemeCell(const TypographyTheme &theme, bool selected,
                                  QWidget *parent = nullptr)
        : QWidget(parent)
        , m_theme(theme)
        , m_selected(selected)
    {
        setFixedSize(120, 50);
        setCursor(Qt::PointingHandCursor);
        setToolTip(theme.name);
    }

    void setSelected(bool selected)
    {
        if (m_selected != selected) {
            m_selected = selected;
            update();
        }
    }

    QString themeId() const { return m_theme.id; }

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
        const int fontSize = 9;

        // Body font: render the body family name
        QFont bodyFont(m_theme.body.family, fontSize);
        p.setFont(bodyFont);
        p.setPen(Qt::black);
        p.drawText(QRect(textMargin, 2, r.width() - 2 * textMargin, 14),
                   Qt::AlignLeft | Qt::AlignVCenter,
                   m_theme.body.family);

        // Heading font: render the heading family name
        QFont headingFont(m_theme.heading.family, fontSize);
        headingFont.setBold(true);
        p.setFont(headingFont);
        p.drawText(QRect(textMargin, 17, r.width() - 2 * textMargin, 14),
                   Qt::AlignLeft | Qt::AlignVCenter,
                   m_theme.heading.family);

        // Mono font: render "mono"
        QFont monoFont(m_theme.mono.family, fontSize - 1);
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
            Q_EMIT clicked(m_theme.id);
        QWidget::mousePressEvent(event);
    }

private:
    TypographyTheme m_theme;
    bool m_selected = false;
};

} // anonymous namespace

// ---------------------------------------------------------------------------
// TypographyThemePickerWidget
// ---------------------------------------------------------------------------

TypographyThemePickerWidget::TypographyThemePickerWidget(TypographyThemeManager *manager, QWidget *parent)
    : QWidget(parent)
    , m_manager(manager)
{
    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(4);

    auto *header = new QLabel(QStringLiteral("Typography"), this);
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

    connect(m_manager, &TypographyThemeManager::themesChanged,
            this, &TypographyThemePickerWidget::refresh);
}

void TypographyThemePickerWidget::setCurrentThemeId(const QString &id)
{
    if (m_currentId == id)
        return;
    m_currentId = id;

    auto cells = findChildren<TypographyThemeCell *>();
    for (auto *cell : cells)
        cell->setSelected(cell->themeId() == m_currentId);
}

void TypographyThemePickerWidget::refresh()
{
    rebuildGrid();
}

void TypographyThemePickerWidget::rebuildGrid()
{
    while (QLayoutItem *item = m_gridLayout->takeAt(0)) {
        delete item->widget();
        delete item;
    }

    if (!m_manager)
        return;

    const QStringList ids = m_manager->availableThemes();
    const int columns = 2;

    int row = 0;
    int col = 0;
    for (const QString &id : ids) {
        TypographyTheme theme = m_manager->theme(id);
        bool selected = (id == m_currentId);
        auto *cell = new TypographyThemeCell(theme, selected, this);
        connect(cell, &TypographyThemeCell::clicked, this, [this](const QString &clickedId) {
            setCurrentThemeId(clickedId);
            Q_EMIT themeSelected(clickedId);
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
    addButton->setToolTip(QStringLiteral("Create new typography theme"));
    connect(addButton, &QToolButton::clicked, this, &TypographyThemePickerWidget::createRequested);
    m_gridLayout->addWidget(addButton, row, col);
}

#include "typographythemepickerwidget.moc"
