// SPDX-License-Identifier: GPL-2.0-or-later

#include "typesetpickerwidget.h"

#include <QFont>
#include <QPainter>

#include "typeset.h"
#include "typesetmanager.h"

namespace {

class TypeSetCell : public ResourcePickerCellBase
{
    Q_OBJECT

public:
    explicit TypeSetCell(const TypeSet &typeSet, bool selected,
                         QWidget *parent = nullptr)
        : ResourcePickerCellBase(typeSet.id, selected, parent)
        , m_typeSet(typeSet)
    {
        setFixedSize(120, 62);
        setToolTip(typeSet.name);
    }

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

        drawSelectionBorder(p);
    }

private:
    TypeSet m_typeSet;
};

} // anonymous namespace

TypeSetPickerWidget::TypeSetPickerWidget(TypeSetManager *manager, QWidget *parent)
    : ResourcePickerWidget(QStringLiteral("Type Sets"), parent)
    , m_manager(manager)
{
    rebuildGrid();
    connect(m_manager, &TypeSetManager::typeSetsChanged,
            this, &TypeSetPickerWidget::refresh);
}

void TypeSetPickerWidget::populateGrid()
{
    if (!m_manager)
        return;

    const QStringList ids = m_manager->availableTypeSets();
    for (const QString &id : ids) {
        TypeSet ts = m_manager->typeSet(id);
        addCell(new TypeSetCell(ts, id == m_currentId, this));
    }
}

#include "typesetpickerwidget.moc"
