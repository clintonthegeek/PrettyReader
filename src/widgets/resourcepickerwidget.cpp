// SPDX-License-Identifier: GPL-2.0-or-later

#include "resourcepickerwidget.h"

#include <QGridLayout>
#include <QLabel>
#include <QVBoxLayout>

ResourcePickerWidget::ResourcePickerWidget(const QString &headerText, QWidget *parent)
    : QWidget(parent)
{
    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(4);

    auto *header = new QLabel(headerText, this);
    QFont headerFont = header->font();
    headerFont.setBold(true);
    header->setFont(headerFont);
    outerLayout->addWidget(header);

    auto *gridContainer = new QWidget(this);
    m_gridLayout = new QGridLayout(gridContainer);
    m_gridLayout->setContentsMargins(0, 0, 0, 0);
    m_gridLayout->setSpacing(4);
    outerLayout->addWidget(gridContainer);

    outerLayout->addStretch();
}

void ResourcePickerWidget::setCurrentId(const QString &id)
{
    if (m_currentId == id)
        return;
    m_currentId = id;

    auto cells = findChildren<ResourcePickerCellBase *>();
    for (auto *cell : cells)
        cell->setSelected(cell->cellId() == m_currentId);
}

void ResourcePickerWidget::refresh()
{
    rebuildGrid();
}

void ResourcePickerWidget::rebuildGrid()
{
    while (QLayoutItem *item = m_gridLayout->takeAt(0)) {
        delete item->widget();
        delete item;
    }

    m_row = 0;
    m_col = 0;
    populateGrid();
}

void ResourcePickerWidget::addCell(ResourcePickerCellBase *cell)
{
    connect(cell, &ResourcePickerCellBase::clicked, this, [this](const QString &id) {
        setCurrentId(id);
        Q_EMIT resourceSelected(id);
    });
    connect(cell, &ResourcePickerCellBase::doubleClicked, this, [this](const QString &id) {
        setCurrentId(id);
        Q_EMIT resourceSelected(id);
        Q_EMIT resourceDoubleClicked(id);
    });
    m_gridLayout->addWidget(cell, m_row, m_col);
    if (++m_col >= gridColumns()) {
        m_col = 0;
        ++m_row;
    }
}
