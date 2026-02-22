/*
 * paletteeditordialog.cpp — Editor dialog for color palettes
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "paletteeditordialog.h"
#include "colorpalette.h"

#include <KColorButton>

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QVBoxLayout>

PaletteEditorDialog::PaletteEditorDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Edit Color Palette"));
    resize(480, 600);
    buildUI();
}

void PaletteEditorDialog::buildUI()
{
    auto *mainLayout = new QVBoxLayout(this);

    // --- Name field ---
    auto *nameLayout = new QFormLayout;
    m_nameEdit = new QLineEdit;
    m_nameEdit->setPlaceholderText(tr("e.g. My Custom Palette"));
    nameLayout->addRow(tr("Name:"), m_nameEdit);
    mainLayout->addLayout(nameLayout);

    // --- Helper to add a color role row ---
    auto addColorRow = [this](QFormLayout *form, const QString &label, const QString &role,
                              const QColor &defaultColor) {
        auto *btn = new KColorButton;
        btn->setColor(defaultColor);
        m_colorButtons.insert(role, btn);
        form->addRow(label, btn);

        connect(btn, &KColorButton::changed, this, [this]() {
            updatePreviewStrip();
        });
    };

    // --- Text Colors group ---
    auto *textGroup = new QGroupBox(tr("Text Colors"));
    auto *textForm = new QFormLayout(textGroup);
    addColorRow(textForm, tr("Text:"), QStringLiteral("text"), QColor(0x33, 0x33, 0x33));
    addColorRow(textForm, tr("Heading:"), QStringLiteral("headingText"), QColor(0x1a, 0x1a, 0x2e));
    addColorRow(textForm, tr("Blockquote:"), QStringLiteral("blockquoteText"), QColor(0x55, 0x55, 0x55));
    addColorRow(textForm, tr("Link:"), QStringLiteral("linkText"), QColor(0x1a, 0x6b, 0xb8));
    addColorRow(textForm, tr("Code:"), QStringLiteral("codeText"), QColor(0xc7, 0x25, 0x4e));
    mainLayout->addWidget(textGroup);

    // --- Surface Colors group ---
    auto *surfaceGroup = new QGroupBox(tr("Surface Colors"));
    auto *surfaceForm = new QFormLayout(surfaceGroup);
    addColorRow(surfaceForm, tr("Page Background:"), QStringLiteral("pageBackground"), Qt::white);
    addColorRow(surfaceForm, tr("Code Block:"), QStringLiteral("surfaceCode"), QColor(0xf5, 0xf5, 0xf5));
    addColorRow(surfaceForm, tr("Inline Code:"), QStringLiteral("surfaceInlineCode"), QColor(0xf0, 0xf0, 0xf0));
    addColorRow(surfaceForm, tr("Table Header:"), QStringLiteral("surfaceTableHeader"), QColor(0xf0, 0xf0, 0xf0));
    addColorRow(surfaceForm, tr("Table Alt Row:"), QStringLiteral("surfaceTableAlt"), QColor(0xfa, 0xfa, 0xfa));
    mainLayout->addWidget(surfaceGroup);

    // --- Border Colors group ---
    auto *borderGroup = new QGroupBox(tr("Border Colors"));
    auto *borderForm = new QFormLayout(borderGroup);
    addColorRow(borderForm, tr("Outer:"), QStringLiteral("borderOuter"), QColor(0xdd, 0xdd, 0xdd));
    addColorRow(borderForm, tr("Inner:"), QStringLiteral("borderInner"), QColor(0xee, 0xee, 0xee));
    addColorRow(borderForm, tr("Header Bottom:"), QStringLiteral("borderHeaderBottom"), QColor(0xcc, 0xcc, 0xcc));
    mainLayout->addWidget(borderGroup);

    // --- Live preview strip ---
    m_previewStrip = new QWidget;
    m_previewStrip->setFixedHeight(48);
    m_previewStrip->setMinimumWidth(200);
    mainLayout->addWidget(m_previewStrip);
    updatePreviewStrip();

    // --- Button box ---
    m_buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    mainLayout->addWidget(m_buttonBox);

    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void PaletteEditorDialog::updatePreviewStrip()
{
    // Build a pixmap showing representative colors
    const int w = m_previewStrip->width() > 0 ? m_previewStrip->width() : 400;
    const int h = 48;

    QPixmap pm(w, h);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, false);

    auto colorFor = [this](const QString &role) -> QColor {
        auto *btn = m_colorButtons.value(role);
        return btn ? btn->color() : Qt::gray;
    };

    // Page background as base
    p.fillRect(0, 0, w, h, colorFor(QStringLiteral("pageBackground")));

    // Draw color swatches across the width
    struct SwatchInfo {
        QString role;
        QString label;
    };
    const QList<SwatchInfo> swatches = {
        {QStringLiteral("text"),               QStringLiteral("Aa")},
        {QStringLiteral("headingText"),        QStringLiteral("H1")},
        {QStringLiteral("linkText"),           QStringLiteral("Lk")},
        {QStringLiteral("codeText"),           QStringLiteral("Cd")},
        {QStringLiteral("surfaceCode"),        QString()},
        {QStringLiteral("surfaceInlineCode"),  QString()},
        {QStringLiteral("borderOuter"),        QString()},
        {QStringLiteral("borderInner"),        QString()},
    };

    const int swatchWidth = w / swatches.size();
    for (int i = 0; i < swatches.size(); ++i) {
        const QColor c = colorFor(swatches[i].role);
        const QRect r(i * swatchWidth, 4, swatchWidth - 2, h - 8);
        p.fillRect(r, c);

        // Draw label in contrasting color if present
        if (!swatches[i].label.isEmpty()) {
            // Simple contrast: if luminance > 128, use black; else white
            const int lum = (c.red() * 299 + c.green() * 587 + c.blue() * 114) / 1000;
            p.setPen(lum > 128 ? Qt::black : Qt::white);
            QFont f = p.font();
            f.setPointSize(9);
            f.setBold(true);
            p.setFont(f);
            p.drawText(r, Qt::AlignCenter, swatches[i].label);
        }
    }

    p.end();

    // Apply as background via stylesheet + label trick
    QPalette pal = m_previewStrip->palette();
    pal.setBrush(QPalette::Window, QBrush(pm));
    m_previewStrip->setPalette(pal);
    m_previewStrip->setAutoFillBackground(true);
    m_previewStrip->update();
}

void PaletteEditorDialog::setColorPalette(const ColorPalette &palette)
{
    m_nameEdit->setText(palette.name);

    for (auto it = palette.colors.constBegin(); it != palette.colors.constEnd(); ++it) {
        auto *btn = m_colorButtons.value(it.key());
        if (btn)
            btn->setColor(it.value());
    }

    updatePreviewStrip();
}

ColorPalette PaletteEditorDialog::colorPalette() const
{
    ColorPalette pal;
    pal.name = m_nameEdit->text().trimmed();

    for (auto it = m_colorButtons.constBegin(); it != m_colorButtons.constEnd(); ++it) {
        pal.colors.insert(it.key(), it.value()->color());
    }

    return pal;
}
