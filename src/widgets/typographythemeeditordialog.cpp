/*
 * typographythemeeditordialog.cpp — Editor dialog for typography themes
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "typographythemeeditordialog.h"
#include "typographytheme.h"
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

TypographyThemeEditorDialog::TypographyThemeEditorDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Edit Typography Theme"));
    resize(550, 400);
    buildUI();
}

void TypographyThemeEditorDialog::buildUI()
{
    HersheyFontRegistry::instance().ensureLoaded();
    const QStringList hersheyFamilies = HersheyFontRegistry::instance().familyNames();

    auto *mainLayout = new QVBoxLayout(this);

    // --- Name field ---
    auto *nameLayout = new QFormLayout;
    m_nameEdit = new QLineEdit;
    m_nameEdit->setPlaceholderText(tr("e.g. My Custom Theme"));
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

void TypographyThemeEditorDialog::updatePreview(QLabel *preview, QFontComboBox *fontCombo)
{
    QFont previewFont = fontCombo->currentFont();
    previewFont.setPointSize(13);
    preview->setFont(previewFont);
}

void TypographyThemeEditorDialog::setTypographyTheme(const TypographyTheme &theme)
{
    m_nameEdit->setText(theme.name);

    // Body
    m_bodyFontCombo->setCurrentFont(QFont(theme.body.family));
    int idx = m_bodyHersheyCombo->findText(theme.body.hersheyFamily);
    if (idx >= 0)
        m_bodyHersheyCombo->setCurrentIndex(idx);

    // Heading
    m_headingFontCombo->setCurrentFont(QFont(theme.heading.family));
    idx = m_headingHersheyCombo->findText(theme.heading.hersheyFamily);
    if (idx >= 0)
        m_headingHersheyCombo->setCurrentIndex(idx);

    // Mono
    m_monoFontCombo->setCurrentFont(QFont(theme.mono.family));
    idx = m_monoHersheyCombo->findText(theme.mono.hersheyFamily);
    if (idx >= 0)
        m_monoHersheyCombo->setCurrentIndex(idx);
}

TypographyTheme TypographyThemeEditorDialog::typographyTheme() const
{
    TypographyTheme theme;
    theme.name = m_nameEdit->text().trimmed();

    theme.body.family = m_bodyFontCombo->currentFont().family();
    theme.body.hersheyFamily = m_bodyHersheyCombo->currentText();

    theme.heading.family = m_headingFontCombo->currentFont().family();
    theme.heading.hersheyFamily = m_headingHersheyCombo->currentText();

    theme.mono.family = m_monoFontCombo->currentFont().family();
    theme.mono.hersheyFamily = m_monoHersheyCombo->currentText();

    return theme;
}
