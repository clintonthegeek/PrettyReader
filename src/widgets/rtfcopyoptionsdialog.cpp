/*
 * rtfcopyoptionsdialog.cpp — Dialog for choosing RTF copy style options
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "rtfcopyoptionsdialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>

// Preset indices matching the combo box order
enum PresetIndex {
    FullStyle = 0,
    NoColors,
    FontsAndSizes,
    StructureOnly,
    Custom
};

RtfCopyOptionsDialog::RtfCopyOptionsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(i18n("Copy with Style Options"));
    setMinimumWidth(360);

    auto *mainLayout = new QVBoxLayout(this);

    // --- Preset combo ---
    auto *presetLayout = new QHBoxLayout;
    presetLayout->addWidget(new QLabel(i18n("Preset:")));
    m_presetCombo = new QComboBox;
    m_presetCombo->addItem(i18n("Full Style"));
    m_presetCombo->addItem(i18n("No Colors"));
    m_presetCombo->addItem(i18n("Fonts and Sizes"));
    m_presetCombo->addItem(i18n("Structure Only"));
    m_presetCombo->addItem(i18n("Custom"));
    presetLayout->addWidget(m_presetCombo, 1);
    mainLayout->addLayout(presetLayout);

    // --- Character Formatting group ---
    auto *charGroup = new QGroupBox(i18n("Character Formatting"));
    auto *charLayout = new QVBoxLayout(charGroup);

    m_fontsCb = new QCheckBox(i18n("Fonts and sizes"));
    m_emphasisCb = new QCheckBox(i18n("Bold, italic, underline"));
    m_scriptsCb = new QCheckBox(i18n("Superscript / subscript"));
    m_textColorCb = new QCheckBox(i18n("Text color"));
    m_highlightsCb = new QCheckBox(i18n("Background / highlight colors"));
    m_sourceFormattingCb = new QCheckBox(i18n("Preserve source formatting"));
    m_sourceFormattingCb->setToolTip(
        i18n("Per-word bold, code styles, links.\n"
             "When off, all text in a block uses uniform base style."));

    charLayout->addWidget(m_fontsCb);
    charLayout->addWidget(m_emphasisCb);
    charLayout->addWidget(m_scriptsCb);
    charLayout->addWidget(m_textColorCb);
    charLayout->addWidget(m_highlightsCb);
    charLayout->addWidget(m_sourceFormattingCb);

    // Indent the description label under source formatting
    auto *sfDescLabel = new QLabel(
        i18n("<small>(per-word bold, code styles, links)</small>"));
    sfDescLabel->setTextFormat(Qt::RichText);
    sfDescLabel->setIndent(20);
    charLayout->addWidget(sfDescLabel);

    mainLayout->addWidget(charGroup);

    // --- Paragraph Formatting group ---
    auto *paraGroup = new QGroupBox(i18n("Paragraph Formatting"));
    auto *paraLayout = new QVBoxLayout(paraGroup);

    m_alignmentCb = new QCheckBox(i18n("Alignment"));
    m_spacingCb = new QCheckBox(i18n("Spacing (before, after, line height)"));
    m_marginsCb = new QCheckBox(i18n("Margins and indents"));

    paraLayout->addWidget(m_alignmentCb);
    paraLayout->addWidget(m_spacingCb);
    paraLayout->addWidget(m_marginsCb);

    mainLayout->addWidget(paraGroup);

    // --- Button box ---
    auto *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Cancel);
    auto *copyButton = buttonBox->addButton(
        i18n("Copy"), QDialogButtonBox::AcceptRole);
    copyButton->setIcon(QIcon::fromTheme(QStringLiteral("edit-copy")));
    copyButton->setDefault(true);
    mainLayout->addWidget(buttonBox);

    connect(buttonBox, &QDialogButtonBox::accepted, this, [this]() {
        saveSettings();
        accept();
    });
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // Connections
    connect(m_presetCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &RtfCopyOptionsDialog::onPresetChanged);

    auto connectCb = [this](QCheckBox *cb) {
        connect(cb, &QCheckBox::toggled,
                this, &RtfCopyOptionsDialog::onCheckboxToggled);
    };
    connectCb(m_fontsCb);
    connectCb(m_emphasisCb);
    connectCb(m_scriptsCb);
    connectCb(m_textColorCb);
    connectCb(m_highlightsCb);
    connectCb(m_sourceFormattingCb);
    connectCb(m_alignmentCb);
    connectCb(m_spacingCb);
    connectCb(m_marginsCb);

    loadSettings();
}

RtfFilterOptions RtfCopyOptionsDialog::filterOptions() const
{
    return checkboxesToFilter();
}

void RtfCopyOptionsDialog::onPresetChanged(int index)
{
    if (index == Custom)
        return; // user toggled checkboxes manually

    RtfFilterOptions opts;
    switch (index) {
    case FullStyle:     opts = RtfFilterOptions::fullStyle(); break;
    case NoColors:      opts = RtfFilterOptions::noColors(); break;
    case FontsAndSizes: opts = RtfFilterOptions::fontsAndSizes(); break;
    case StructureOnly: opts = RtfFilterOptions::structureOnly(); break;
    default: return;
    }

    blockCheckboxSignals(true);
    applyFilterToCheckboxes(opts);
    blockCheckboxSignals(false);
}

void RtfCopyOptionsDialog::onCheckboxToggled()
{
    // If user manually toggles any checkbox, switch preset to "Custom"
    // unless the current state happens to match a known preset.
    RtfFilterOptions current = checkboxesToFilter();

    auto matches = [&](const RtfFilterOptions &ref) {
        return current.includeFonts == ref.includeFonts
            && current.includeEmphasis == ref.includeEmphasis
            && current.includeScripts == ref.includeScripts
            && current.includeTextColor == ref.includeTextColor
            && current.includeHighlights == ref.includeHighlights
            && current.includeAlignment == ref.includeAlignment
            && current.includeSpacing == ref.includeSpacing
            && current.includeMargins == ref.includeMargins
            && current.includeSourceFormatting == ref.includeSourceFormatting;
    };

    const QSignalBlocker blocker(m_presetCombo);
    if (matches(RtfFilterOptions::fullStyle()))
        m_presetCombo->setCurrentIndex(FullStyle);
    else if (matches(RtfFilterOptions::noColors()))
        m_presetCombo->setCurrentIndex(NoColors);
    else if (matches(RtfFilterOptions::fontsAndSizes()))
        m_presetCombo->setCurrentIndex(FontsAndSizes);
    else if (matches(RtfFilterOptions::structureOnly()))
        m_presetCombo->setCurrentIndex(StructureOnly);
    else
        m_presetCombo->setCurrentIndex(Custom);
}

void RtfCopyOptionsDialog::loadSettings()
{
    KConfigGroup group(KSharedConfig::openConfig(),
                       QStringLiteral("CopyOptions"));

    RtfFilterOptions opts;
    opts.includeFonts       = group.readEntry("CopyIncludeFonts", true);
    opts.includeEmphasis    = group.readEntry("CopyIncludeEmphasis", true);
    opts.includeScripts     = group.readEntry("CopyIncludeScripts", true);
    opts.includeTextColor   = group.readEntry("CopyIncludeTextColor", true);
    opts.includeHighlights  = group.readEntry("CopyIncludeHighlights", true);
    opts.includeAlignment   = group.readEntry("CopyIncludeAlignment", true);
    opts.includeSpacing     = group.readEntry("CopyIncludeSpacing", true);
    opts.includeMargins     = group.readEntry("CopyIncludeMargins", true);
    opts.includeSourceFormatting = group.readEntry("CopyIncludeSourceFormatting", true);

    blockCheckboxSignals(true);
    applyFilterToCheckboxes(opts);
    blockCheckboxSignals(false);

    // Set the preset combo to match
    QString presetName = group.readEntry("CopyPreset", QStringLiteral("Full Style"));
    if (presetName == QLatin1String("Full Style"))
        m_presetCombo->setCurrentIndex(FullStyle);
    else if (presetName == QLatin1String("No Colors"))
        m_presetCombo->setCurrentIndex(NoColors);
    else if (presetName == QLatin1String("Fonts and Sizes"))
        m_presetCombo->setCurrentIndex(FontsAndSizes);
    else if (presetName == QLatin1String("Structure Only"))
        m_presetCombo->setCurrentIndex(StructureOnly);
    else
        m_presetCombo->setCurrentIndex(Custom);
}

void RtfCopyOptionsDialog::saveSettings()
{
    KConfigGroup group(KSharedConfig::openConfig(),
                       QStringLiteral("CopyOptions"));

    RtfFilterOptions opts = checkboxesToFilter();
    group.writeEntry("CopyIncludeFonts", opts.includeFonts);
    group.writeEntry("CopyIncludeEmphasis", opts.includeEmphasis);
    group.writeEntry("CopyIncludeScripts", opts.includeScripts);
    group.writeEntry("CopyIncludeTextColor", opts.includeTextColor);
    group.writeEntry("CopyIncludeHighlights", opts.includeHighlights);
    group.writeEntry("CopyIncludeAlignment", opts.includeAlignment);
    group.writeEntry("CopyIncludeSpacing", opts.includeSpacing);
    group.writeEntry("CopyIncludeMargins", opts.includeMargins);
    group.writeEntry("CopyIncludeSourceFormatting", opts.includeSourceFormatting);
    group.writeEntry("CopyPreset", m_presetCombo->currentText());
    group.sync();
}

void RtfCopyOptionsDialog::applyFilterToCheckboxes(const RtfFilterOptions &opts)
{
    m_fontsCb->setChecked(opts.includeFonts);
    m_emphasisCb->setChecked(opts.includeEmphasis);
    m_scriptsCb->setChecked(opts.includeScripts);
    m_textColorCb->setChecked(opts.includeTextColor);
    m_highlightsCb->setChecked(opts.includeHighlights);
    m_sourceFormattingCb->setChecked(opts.includeSourceFormatting);
    m_alignmentCb->setChecked(opts.includeAlignment);
    m_spacingCb->setChecked(opts.includeSpacing);
    m_marginsCb->setChecked(opts.includeMargins);
}

RtfFilterOptions RtfCopyOptionsDialog::checkboxesToFilter() const
{
    RtfFilterOptions opts;
    opts.includeFonts       = m_fontsCb->isChecked();
    opts.includeEmphasis    = m_emphasisCb->isChecked();
    opts.includeScripts     = m_scriptsCb->isChecked();
    opts.includeTextColor   = m_textColorCb->isChecked();
    opts.includeHighlights  = m_highlightsCb->isChecked();
    opts.includeSourceFormatting = m_sourceFormattingCb->isChecked();
    opts.includeAlignment   = m_alignmentCb->isChecked();
    opts.includeSpacing     = m_spacingCb->isChecked();
    opts.includeMargins     = m_marginsCb->isChecked();
    return opts;
}

void RtfCopyOptionsDialog::blockCheckboxSignals(bool block)
{
    m_fontsCb->blockSignals(block);
    m_emphasisCb->blockSignals(block);
    m_scriptsCb->blockSignals(block);
    m_textColorCb->blockSignals(block);
    m_highlightsCb->blockSignals(block);
    m_sourceFormattingCb->blockSignals(block);
    m_alignmentCb->blockSignals(block);
    m_spacingCb->blockSignals(block);
    m_marginsCb->blockSignals(block);
}
