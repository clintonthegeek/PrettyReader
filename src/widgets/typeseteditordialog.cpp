/*
 * typeseteditordialog.cpp — Editor dialog for type sets
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "typeseteditordialog.h"
#include "typeset.h"
#include "hersheyfont.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFont>
#include <QFontComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

static const QString s_sampleText = QStringLiteral("The quick brown fox jumps over the lazy dog.");

TypeSetEditorDialog::TypeSetEditorDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Edit Type Set"));
    resize(550, 400);
    buildUI();
}

void TypeSetEditorDialog::buildUI()
{
    HersheyFontRegistry::instance().ensureLoaded();
    const QStringList hersheyFamilies = HersheyFontRegistry::instance().familyNames();

    auto *mainLayout = new QVBoxLayout(this);

    // --- Name field ---
    auto *nameLayout = new QFormLayout;
    m_nameEdit = new QLineEdit;
    m_nameEdit->setPlaceholderText(tr("e.g. My Custom Type Set"));
    nameLayout->addRow(tr("Name:"), m_nameEdit);
    mainLayout->addLayout(nameLayout);

    // --- Helper to create a font role group ---
    auto createRoleGroup = [&](const QString &title,
                               QFontComboBox *&fontCombo,
                               QComboBox *&hersheyCombo,
                               QLabel *&preview,
                               QFontComboBox::FontFilters filters = QFontComboBox::AllFonts) {
        auto *group = new QGroupBox(title);
        auto *groupLayout = new QVBoxLayout(group);

        auto *row = new QHBoxLayout;

        fontCombo = new QFontComboBox;
        fontCombo->setFontFilters(filters);
        row->addWidget(fontCombo, 1);

        hersheyCombo = new QComboBox;
        hersheyCombo->addItems(hersheyFamilies);
        row->addWidget(hersheyCombo, 1);

        groupLayout->addLayout(row);

        preview = new QLabel(s_sampleText);
        preview->setWordWrap(true);
        preview->setMinimumHeight(30);
        preview->setFrameShape(QFrame::StyledPanel);
        preview->setMargin(4);
        groupLayout->addWidget(preview);

        connect(fontCombo, &QFontComboBox::currentFontChanged,
                this, [this, preview, fontCombo]() {
            updatePreview(preview, fontCombo);
        });

        updatePreview(preview, fontCombo);

        return group;
    };

    // --- Body ---
    mainLayout->addWidget(
        createRoleGroup(tr("Body"), m_bodyFontCombo, m_bodyHersheyCombo, m_bodyPreview));

    // --- Heading ---
    mainLayout->addWidget(
        createRoleGroup(tr("Heading"), m_headingFontCombo, m_headingHersheyCombo, m_headingPreview));

    // --- Mono ---
    mainLayout->addWidget(
        createRoleGroup(tr("Mono"), m_monoFontCombo, m_monoHersheyCombo, m_monoPreview,
                        QFontComboBox::MonospacedFonts));

    // --- Button box ---
    m_buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    mainLayout->addWidget(m_buttonBox);

    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void TypeSetEditorDialog::updatePreview(QLabel *preview, QFontComboBox *fontCombo)
{
    QFont previewFont = fontCombo->currentFont();
    previewFont.setPointSize(13);
    preview->setFont(previewFont);
}

void TypeSetEditorDialog::setTypeSet(const TypeSet &typeSet)
{
    m_nameEdit->setText(typeSet.name);

    // Body
    m_bodyFontCombo->setCurrentFont(QFont(typeSet.body.family));
    int idx = m_bodyHersheyCombo->findText(typeSet.body.hersheyFamily);
    if (idx >= 0)
        m_bodyHersheyCombo->setCurrentIndex(idx);

    // Heading
    m_headingFontCombo->setCurrentFont(QFont(typeSet.heading.family));
    idx = m_headingHersheyCombo->findText(typeSet.heading.hersheyFamily);
    if (idx >= 0)
        m_headingHersheyCombo->setCurrentIndex(idx);

    // Mono
    m_monoFontCombo->setCurrentFont(QFont(typeSet.mono.family));
    idx = m_monoHersheyCombo->findText(typeSet.mono.hersheyFamily);
    if (idx >= 0)
        m_monoHersheyCombo->setCurrentIndex(idx);
}

TypeSet TypeSetEditorDialog::typeSet() const
{
    TypeSet ts;
    ts.name = m_nameEdit->text().trimmed();

    ts.body.family = m_bodyFontCombo->currentFont().family();
    ts.body.hersheyFamily = m_bodyHersheyCombo->currentText();

    ts.heading.family = m_headingFontCombo->currentFont().family();
    ts.heading.hersheyFamily = m_headingHersheyCombo->currentText();

    ts.mono.family = m_monoFontCombo->currentFont().family();
    ts.mono.hersheyFamily = m_monoHersheyCombo->currentText();

    return ts;
}
