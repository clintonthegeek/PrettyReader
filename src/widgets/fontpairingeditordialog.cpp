/*
 * fontpairingeditordialog.cpp — Editor dialog for font pairings
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "fontpairingeditordialog.h"
#include "fontpairing.h"
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

FontPairingEditorDialog::FontPairingEditorDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Edit Font Pairing"));
    resize(550, 400);
    buildUI();
}

void FontPairingEditorDialog::buildUI()
{
    // Ensure Hershey fonts are available
    HersheyFontRegistry::instance().ensureLoaded();
    const QStringList hersheyFamilies = HersheyFontRegistry::instance().familyNames();

    auto *mainLayout = new QVBoxLayout(this);

    // --- Name field ---
    auto *nameLayout = new QFormLayout;
    m_nameEdit = new QLineEdit;
    m_nameEdit->setPlaceholderText(tr("e.g. My Custom Pairing"));
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

        // Live preview update
        connect(fontCombo, &QFontComboBox::currentFontChanged,
                this, [this, preview, fontCombo]() {
            updatePreview(preview, fontCombo);
        });

        // Initialize preview
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

void FontPairingEditorDialog::updatePreview(QLabel *preview, QFontComboBox *fontCombo)
{
    QFont previewFont = fontCombo->currentFont();
    previewFont.setPointSize(13);
    preview->setFont(previewFont);
}

void FontPairingEditorDialog::setFontPairing(const FontPairing &fp)
{
    m_nameEdit->setText(fp.name);

    // Body
    m_bodyFontCombo->setCurrentFont(QFont(fp.body.family));
    int idx = m_bodyHersheyCombo->findText(fp.body.hersheyFamily);
    if (idx >= 0)
        m_bodyHersheyCombo->setCurrentIndex(idx);

    // Heading
    m_headingFontCombo->setCurrentFont(QFont(fp.heading.family));
    idx = m_headingHersheyCombo->findText(fp.heading.hersheyFamily);
    if (idx >= 0)
        m_headingHersheyCombo->setCurrentIndex(idx);

    // Mono
    m_monoFontCombo->setCurrentFont(QFont(fp.mono.family));
    idx = m_monoHersheyCombo->findText(fp.mono.hersheyFamily);
    if (idx >= 0)
        m_monoHersheyCombo->setCurrentIndex(idx);
}

FontPairing FontPairingEditorDialog::fontPairing() const
{
    FontPairing fp;
    fp.name = m_nameEdit->text().trimmed();

    fp.body.family = m_bodyFontCombo->currentFont().family();
    fp.body.hersheyFamily = m_bodyHersheyCombo->currentText();

    fp.heading.family = m_headingFontCombo->currentFont().family();
    fp.heading.hersheyFamily = m_headingHersheyCombo->currentText();

    fp.mono.family = m_monoFontCombo->currentFont().family();
    fp.mono.hersheyFamily = m_monoHersheyCombo->currentText();

    return fp;
}
