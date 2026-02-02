/*
 * languagepickerdialog.cpp — Searchable syntax-language picker
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "languagepickerdialog.h"

#include <QDialogButtonBox>
#include <QLineEdit>
#include <QListWidget>
#include <QVBoxLayout>

#include <KSyntaxHighlighting/Definition>
#include <KSyntaxHighlighting/Repository>

#include <algorithm>

// Item data roles
static constexpr int LanguageNameRole = Qt::UserRole;
static constexpr int IsSectionHeaderRole = Qt::UserRole + 1;
static constexpr int SectionNameRole = Qt::UserRole + 2;

LanguagePickerDialog::LanguagePickerDialog(const QString &currentLanguage,
                                           QWidget *parent)
    : QDialog(parent)
    , m_currentLanguage(currentLanguage)
{
    setWindowTitle(tr("Select Syntax Language"));
    resize(400, 500);

    auto *layout = new QVBoxLayout(this);

    m_filterEdit = new QLineEdit(this);
    m_filterEdit->setPlaceholderText(tr("Type to filter..."));
    m_filterEdit->setClearButtonEnabled(true);
    layout->addWidget(m_filterEdit);

    m_listWidget = new QListWidget(this);
    layout->addWidget(m_listWidget);

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_filterEdit, &QLineEdit::textChanged,
            this, &LanguagePickerDialog::onFilterChanged);
    connect(m_listWidget, &QListWidget::itemDoubleClicked,
            this, &LanguagePickerDialog::onItemDoubleClicked);

    populate();
    selectLanguage(m_currentLanguage);

    m_filterEdit->setFocus();
}

QString LanguagePickerDialog::selectedLanguage() const
{
    auto *item = m_listWidget->currentItem();
    if (!item || item->data(IsSectionHeaderRole).toBool())
        return {};
    return item->data(LanguageNameRole).toString();
}

void LanguagePickerDialog::populate()
{
    static KSyntaxHighlighting::Repository repo;

    m_listWidget->clear();

    // "None (plain text)" as first entry
    auto *noneItem = new QListWidgetItem(tr("None (plain text)"));
    noneItem->setData(LanguageNameRole, QString());
    noneItem->setData(IsSectionHeaderRole, false);
    m_listWidget->addItem(noneItem);

    // Collect and sort definitions by section then translatedName
    auto defs = repo.definitions();
    std::sort(defs.begin(), defs.end(),
              [](const KSyntaxHighlighting::Definition &a,
                 const KSyntaxHighlighting::Definition &b) {
        int cmp = a.section().compare(b.section(), Qt::CaseInsensitive);
        if (cmp != 0)
            return cmp < 0;
        return a.translatedName().compare(b.translatedName(), Qt::CaseInsensitive) < 0;
    });

    QString currentSection;
    for (const auto &def : defs) {
        if (!def.isValid())
            continue;

        // Section header
        if (def.section() != currentSection) {
            currentSection = def.section();
            auto *header = new QListWidgetItem(currentSection);
            header->setData(IsSectionHeaderRole, true);
            header->setData(SectionNameRole, currentSection);
            QFont font = header->font();
            font.setBold(true);
            header->setFont(font);
            header->setFlags(Qt::NoItemFlags); // non-selectable
            m_listWidget->addItem(header);
        }

        // Language entry
        auto *item = new QListWidgetItem(
            QStringLiteral("    ") + def.translatedName());
        item->setData(LanguageNameRole, def.name());
        item->setData(IsSectionHeaderRole, false);
        item->setData(SectionNameRole, currentSection);
        m_listWidget->addItem(item);
    }
}

void LanguagePickerDialog::onFilterChanged(const QString &text)
{
    applyFilter(text);
}

void LanguagePickerDialog::applyFilter(const QString &filter)
{
    static KSyntaxHighlighting::Repository repo;

    if (filter.isEmpty()) {
        // Show all items
        for (int i = 0; i < m_listWidget->count(); ++i)
            m_listWidget->item(i)->setHidden(false);
        return;
    }

    // Build a set of sections that have at least one visible child
    QSet<QString> visibleSections;

    // First pass: show/hide language items based on filter
    for (int i = 0; i < m_listWidget->count(); ++i) {
        auto *item = m_listWidget->item(i);
        if (item->data(IsSectionHeaderRole).toBool())
            continue; // handle headers in second pass

        QString langName = item->data(LanguageNameRole).toString();
        if (langName.isEmpty()) {
            // "None (plain text)" — always visible
            item->setHidden(false);
            continue;
        }

        // Match against definition name and translated name
        auto def = repo.definitionForName(langName);
        bool matches = false;
        if (def.isValid()) {
            matches = def.name().contains(filter, Qt::CaseInsensitive)
                   || def.translatedName().contains(filter, Qt::CaseInsensitive);
        }

        item->setHidden(!matches);
        if (matches)
            visibleSections.insert(item->data(SectionNameRole).toString());
    }

    // Second pass: show/hide section headers
    for (int i = 0; i < m_listWidget->count(); ++i) {
        auto *item = m_listWidget->item(i);
        if (!item->data(IsSectionHeaderRole).toBool())
            continue;
        QString section = item->data(SectionNameRole).toString();
        item->setHidden(!visibleSections.contains(section));
    }
}

void LanguagePickerDialog::selectLanguage(const QString &language)
{
    for (int i = 0; i < m_listWidget->count(); ++i) {
        auto *item = m_listWidget->item(i);
        if (item->data(IsSectionHeaderRole).toBool())
            continue;
        if (item->data(LanguageNameRole).toString() == language) {
            m_listWidget->setCurrentItem(item);
            m_listWidget->scrollToItem(item, QAbstractItemView::PositionAtCenter);
            return;
        }
    }
    // Default: select "None"
    if (m_listWidget->count() > 0)
        m_listWidget->setCurrentRow(0);
}

void LanguagePickerDialog::onItemDoubleClicked(QListWidgetItem *item)
{
    if (!item || item->data(IsSectionHeaderRole).toBool())
        return;
    accept();
}
