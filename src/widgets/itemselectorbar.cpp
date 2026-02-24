// SPDX-License-Identifier: GPL-2.0-or-later

#include "itemselectorbar.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QSignalBlocker>
#include <QToolButton>

ItemSelectorBar::ItemSelectorBar(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    m_combo = new QComboBox;
    m_combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    layout->addWidget(m_combo, 1);

    m_duplicateBtn = new QToolButton;
    m_duplicateBtn->setIcon(QIcon::fromTheme(QStringLiteral("edit-copy")));
    m_duplicateBtn->setToolTip(tr("Duplicate"));
    m_duplicateBtn->setAutoRaise(true);
    layout->addWidget(m_duplicateBtn);

    m_saveBtn = new QToolButton;
    m_saveBtn->setIcon(QIcon::fromTheme(QStringLiteral("document-save")));
    m_saveBtn->setToolTip(tr("Save"));
    m_saveBtn->setAutoRaise(true);
    layout->addWidget(m_saveBtn);

    m_deleteBtn = new QToolButton;
    m_deleteBtn->setIcon(QIcon::fromTheme(QStringLiteral("edit-delete")));
    m_deleteBtn->setToolTip(tr("Delete"));
    m_deleteBtn->setAutoRaise(true);
    layout->addWidget(m_deleteBtn);

    connect(m_combo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this](int index) {
        if (index >= 0) {
            updateButtonStates();
            Q_EMIT currentItemChanged(m_combo->itemData(index).toString());
        }
    });

    connect(m_duplicateBtn, &QToolButton::clicked,
            this, &ItemSelectorBar::duplicateRequested);
    connect(m_saveBtn, &QToolButton::clicked,
            this, &ItemSelectorBar::saveRequested);
    connect(m_deleteBtn, &QToolButton::clicked,
            this, &ItemSelectorBar::deleteRequested);
}

void ItemSelectorBar::setItems(const QStringList &ids, const QStringList &names,
                               const QStringList &builtinIds)
{
    const QSignalBlocker blocker(m_combo);
    m_builtinIds = builtinIds;

    QString previousId = currentId();
    m_combo->clear();

    for (int i = 0; i < ids.size() && i < names.size(); ++i) {
        if (builtinIds.contains(ids[i])) {
            m_combo->addItem(QIcon::fromTheme(QStringLiteral("object-locked")),
                             names[i], ids[i]);
        } else {
            m_combo->addItem(names[i], ids[i]);
        }
    }

    // Restore previous selection if still present
    int idx = m_combo->findData(previousId);
    if (idx >= 0)
        m_combo->setCurrentIndex(idx);

    updateButtonStates();
}

QString ItemSelectorBar::currentId() const
{
    return m_combo->currentData().toString();
}

void ItemSelectorBar::setCurrentId(const QString &id)
{
    const QSignalBlocker blocker(m_combo);
    int idx = m_combo->findData(id);
    if (idx >= 0) {
        m_combo->setCurrentIndex(idx);
        updateButtonStates();
    }
}

void ItemSelectorBar::updateButtonStates()
{
    bool isBuiltin = m_builtinIds.contains(currentId());
    m_saveBtn->setEnabled(!isBuiltin);
    m_deleteBtn->setEnabled(!isBuiltin);
    // Duplicate is always enabled
    m_duplicateBtn->setEnabled(m_combo->count() > 0);
}
