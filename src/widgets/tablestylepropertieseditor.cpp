#include "tablestylepropertieseditor.h"

#include <KColorButton>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QSpinBox>
#include <QVBoxLayout>

TableStylePropertiesEditor::TableStylePropertiesEditor(QWidget *parent)
    : QWidget(parent)
{
    buildUI();
}

TableStylePropertiesEditor::BorderRow
TableStylePropertiesEditor::createBorderRow(const QString &label,
                                             QWidget *parent)
{
    Q_UNUSED(parent);
    BorderRow row;

    row.widthSpin = new QDoubleSpinBox;
    row.widthSpin->setRange(0.0, 10.0);
    row.widthSpin->setSuffix(tr(" pt"));
    row.widthSpin->setDecimals(1);
    row.widthSpin->setSingleStep(0.5);

    row.colorBtn = new KColorButton;
    row.colorBtn->setColor(QColor(0x33, 0x33, 0x33));

    row.styleCombo = new QComboBox;
    row.styleCombo->addItem(tr("Solid"), static_cast<int>(Qt::SolidLine));
    row.styleCombo->addItem(tr("Dashed"), static_cast<int>(Qt::DashLine));
    row.styleCombo->addItem(tr("Dotted"), static_cast<int>(Qt::DotLine));

    connect(row.widthSpin, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, &TableStylePropertiesEditor::propertyChanged);
    connect(row.colorBtn, &KColorButton::changed,
            this, &TableStylePropertiesEditor::propertyChanged);
    connect(row.styleCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &TableStylePropertiesEditor::propertyChanged);

    return row;
}

void TableStylePropertiesEditor::buildUI()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    // --- Borders ---
    auto *borderGroup = new QGroupBox(tr("Borders"));
    auto *borderLayout = new QVBoxLayout(borderGroup);
    borderLayout->setContentsMargins(6, 6, 6, 6);
    borderLayout->setSpacing(4);

    auto addBorderRow = [&](const QString &label, BorderRow &row) {
        row = createBorderRow(label, borderGroup);
        auto *hbox = new QHBoxLayout;
        hbox->addWidget(new QLabel(label));
        hbox->addWidget(row.widthSpin);
        hbox->addWidget(row.colorBtn);
        hbox->addWidget(row.styleCombo);
        borderLayout->addLayout(hbox);
    };

    addBorderRow(tr("Outer:"), m_outerBorder);
    addBorderRow(tr("Inner:"), m_innerBorder);
    addBorderRow(tr("Header bottom:"), m_headerBottomBorder);

    layout->addWidget(borderGroup);

    // --- Colors ---
    auto *colorGroup = new QGroupBox(tr("Colors"));
    auto *colorLayout = new QVBoxLayout(colorGroup);
    colorLayout->setContentsMargins(6, 6, 6, 6);
    colorLayout->setSpacing(4);

    auto addColorRow = [&](const QString &label, QCheckBox *&check,
                           KColorButton *&btn) {
        check = new QCheckBox(label);
        btn = new KColorButton;
        auto *hbox = new QHBoxLayout;
        hbox->addWidget(check);
        hbox->addWidget(btn);
        hbox->addStretch();
        colorLayout->addLayout(hbox);

        connect(check, &QCheckBox::toggled, btn, &KColorButton::setEnabled);
        connect(check, &QCheckBox::toggled,
                this, &TableStylePropertiesEditor::propertyChanged);
        connect(btn, &KColorButton::changed,
                this, &TableStylePropertiesEditor::propertyChanged);
    };

    addColorRow(tr("Header bg"), m_headerBgCheck, m_headerBgBtn);
    addColorRow(tr("Header fg"), m_headerFgCheck, m_headerFgBtn);
    addColorRow(tr("Body bg"), m_bodyBgCheck, m_bodyBgBtn);

    // Alternate rows: check + color + frequency
    m_altRowCheck = new QCheckBox(tr("Alt rows"));
    m_altRowBtn = new KColorButton;
    m_altFreqSpin = new QSpinBox;
    m_altFreqSpin->setRange(1, 10);
    m_altFreqSpin->setPrefix(tr("every "));
    m_altFreqSpin->setSuffix(tr(" rows"));

    auto *altRow = new QHBoxLayout;
    altRow->addWidget(m_altRowCheck);
    altRow->addWidget(m_altRowBtn);
    altRow->addWidget(m_altFreqSpin);
    altRow->addStretch();
    colorLayout->addLayout(altRow);

    connect(m_altRowCheck, &QCheckBox::toggled,
            m_altRowBtn, &KColorButton::setEnabled);
    connect(m_altRowCheck, &QCheckBox::toggled,
            m_altFreqSpin, &QSpinBox::setEnabled);
    connect(m_altRowCheck, &QCheckBox::toggled,
            this, &TableStylePropertiesEditor::propertyChanged);
    connect(m_altRowBtn, &KColorButton::changed,
            this, &TableStylePropertiesEditor::propertyChanged);
    connect(m_altFreqSpin, qOverload<int>(&QSpinBox::valueChanged),
            this, &TableStylePropertiesEditor::propertyChanged);

    layout->addWidget(colorGroup);

    // --- Cell Padding ---
    auto *padGroup = new QGroupBox(tr("Cell Padding"));
    auto *padLayout = new QVBoxLayout(padGroup);
    padLayout->setContentsMargins(6, 6, 6, 6);
    padLayout->setSpacing(4);

    auto makePadSpin = [this]() {
        auto *spin = new QDoubleSpinBox;
        spin->setRange(0.0, 20.0);
        spin->setSuffix(tr(" pt"));
        spin->setDecimals(1);
        connect(spin, qOverload<double>(&QDoubleSpinBox::valueChanged),
                this, &TableStylePropertiesEditor::propertyChanged);
        return spin;
    };

    auto *padRow1 = new QHBoxLayout;
    padRow1->addWidget(new QLabel(tr("Top:")));
    m_padTopSpin = makePadSpin();
    padRow1->addWidget(m_padTopSpin);
    padRow1->addWidget(new QLabel(tr("Bottom:")));
    m_padBottomSpin = makePadSpin();
    padRow1->addWidget(m_padBottomSpin);
    padLayout->addLayout(padRow1);

    auto *padRow2 = new QHBoxLayout;
    padRow2->addWidget(new QLabel(tr("Left:")));
    m_padLeftSpin = makePadSpin();
    padRow2->addWidget(m_padLeftSpin);
    padRow2->addWidget(new QLabel(tr("Right:")));
    m_padRightSpin = makePadSpin();
    padRow2->addWidget(m_padRightSpin);
    padLayout->addLayout(padRow2);

    layout->addWidget(padGroup);

    // --- Paragraph Styles ---
    auto *paraGroup = new QGroupBox(tr("Paragraph Styles"));
    auto *paraLayout = new QVBoxLayout(paraGroup);
    paraLayout->setContentsMargins(6, 6, 6, 6);
    paraLayout->setSpacing(4);

    auto *headerParaRow = new QHBoxLayout;
    headerParaRow->addWidget(new QLabel(tr("Header cells:")));
    m_headerParaCombo = new QComboBox;
    headerParaRow->addWidget(m_headerParaCombo, 1);
    paraLayout->addLayout(headerParaRow);

    auto *bodyParaRow = new QHBoxLayout;
    bodyParaRow->addWidget(new QLabel(tr("Body cells:")));
    m_bodyParaCombo = new QComboBox;
    bodyParaRow->addWidget(m_bodyParaCombo, 1);
    paraLayout->addLayout(bodyParaRow);

    connect(m_headerParaCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &TableStylePropertiesEditor::propertyChanged);
    connect(m_bodyParaCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &TableStylePropertiesEditor::propertyChanged);

    layout->addWidget(paraGroup);
    layout->addStretch();
}

void TableStylePropertiesEditor::blockAllSignals(bool block)
{
    m_outerBorder.widthSpin->blockSignals(block);
    m_outerBorder.colorBtn->blockSignals(block);
    m_outerBorder.styleCombo->blockSignals(block);
    m_innerBorder.widthSpin->blockSignals(block);
    m_innerBorder.colorBtn->blockSignals(block);
    m_innerBorder.styleCombo->blockSignals(block);
    m_headerBottomBorder.widthSpin->blockSignals(block);
    m_headerBottomBorder.colorBtn->blockSignals(block);
    m_headerBottomBorder.styleCombo->blockSignals(block);
    m_headerBgCheck->blockSignals(block);
    m_headerBgBtn->blockSignals(block);
    m_headerFgCheck->blockSignals(block);
    m_headerFgBtn->blockSignals(block);
    m_bodyBgCheck->blockSignals(block);
    m_bodyBgBtn->blockSignals(block);
    m_altRowCheck->blockSignals(block);
    m_altRowBtn->blockSignals(block);
    m_altFreqSpin->blockSignals(block);
    m_padTopSpin->blockSignals(block);
    m_padBottomSpin->blockSignals(block);
    m_padLeftSpin->blockSignals(block);
    m_padRightSpin->blockSignals(block);
    m_headerParaCombo->blockSignals(block);
    m_bodyParaCombo->blockSignals(block);
}

void TableStylePropertiesEditor::loadTableStyle(const TableStyle &style,
                                                  const QStringList &paraStyleNames)
{
    blockAllSignals(true);

    auto loadBorder = [](BorderRow &row, const TableStyle::Border &b) {
        row.widthSpin->setValue(b.width);
        row.colorBtn->setColor(b.color);
        int idx = row.styleCombo->findData(static_cast<int>(b.style));
        row.styleCombo->setCurrentIndex(idx >= 0 ? idx : 0);
    };

    loadBorder(m_outerBorder, style.outerBorder());
    loadBorder(m_innerBorder, style.innerBorder());
    loadBorder(m_headerBottomBorder, style.headerBottomBorder());

    m_headerBgCheck->setChecked(style.hasHeaderBackground());
    m_headerBgBtn->setColor(style.headerBackground());
    m_headerBgBtn->setEnabled(style.hasHeaderBackground());

    m_headerFgCheck->setChecked(style.hasHeaderForeground());
    m_headerFgBtn->setColor(style.hasHeaderForeground()
                                ? style.headerForeground() : QColor(Qt::black));
    m_headerFgBtn->setEnabled(style.hasHeaderForeground());

    m_bodyBgCheck->setChecked(style.hasBodyBackground());
    m_bodyBgBtn->setColor(style.bodyBackground());
    m_bodyBgBtn->setEnabled(style.hasBodyBackground());

    m_altRowCheck->setChecked(style.hasAlternateRowColor());
    m_altRowBtn->setColor(style.alternateRowColor());
    m_altRowBtn->setEnabled(style.hasAlternateRowColor());
    m_altFreqSpin->setValue(style.alternateFrequency());
    m_altFreqSpin->setEnabled(style.hasAlternateRowColor());

    QMarginsF pad = style.cellPadding();
    m_padTopSpin->setValue(pad.top());
    m_padBottomSpin->setValue(pad.bottom());
    m_padLeftSpin->setValue(pad.left());
    m_padRightSpin->setValue(pad.right());

    // Paragraph style combos
    m_headerParaCombo->clear();
    m_bodyParaCombo->clear();
    for (const QString &name : paraStyleNames) {
        m_headerParaCombo->addItem(name);
        m_bodyParaCombo->addItem(name);
    }
    int hIdx = m_headerParaCombo->findText(style.headerParagraphStyle());
    m_headerParaCombo->setCurrentIndex(hIdx >= 0 ? hIdx : 0);
    int bIdx = m_bodyParaCombo->findText(style.bodyParagraphStyle());
    m_bodyParaCombo->setCurrentIndex(bIdx >= 0 ? bIdx : 0);

    blockAllSignals(false);
}

void TableStylePropertiesEditor::applyToTableStyle(TableStyle &style) const
{
    auto readBorder = [](const BorderRow &row) {
        TableStyle::Border b;
        b.width = row.widthSpin->value();
        b.color = row.colorBtn->color();
        b.style = static_cast<Qt::PenStyle>(
            row.styleCombo->currentData().toInt());
        return b;
    };

    style.setOuterBorder(readBorder(m_outerBorder));
    style.setInnerBorder(readBorder(m_innerBorder));
    style.setHeaderBottomBorder(readBorder(m_headerBottomBorder));

    if (m_headerBgCheck->isChecked())
        style.setHeaderBackground(m_headerBgBtn->color());
    if (m_headerFgCheck->isChecked())
        style.setHeaderForeground(m_headerFgBtn->color());
    if (m_bodyBgCheck->isChecked())
        style.setBodyBackground(m_bodyBgBtn->color());
    if (m_altRowCheck->isChecked()) {
        style.setAlternateRowColor(m_altRowBtn->color());
        style.setAlternateFrequency(m_altFreqSpin->value());
    }

    style.setCellPadding(QMarginsF(m_padLeftSpin->value(),
                                    m_padTopSpin->value(),
                                    m_padRightSpin->value(),
                                    m_padBottomSpin->value()));

    if (m_headerParaCombo->currentIndex() >= 0)
        style.setHeaderParagraphStyle(m_headerParaCombo->currentText());
    if (m_bodyParaCombo->currentIndex() >= 0)
        style.setBodyParagraphStyle(m_bodyParaCombo->currentText());
}

void TableStylePropertiesEditor::clear()
{
    blockAllSignals(true);

    auto clearBorder = [](BorderRow &row) {
        row.widthSpin->setValue(0.5);
        row.colorBtn->setColor(QColor(0x33, 0x33, 0x33));
        row.styleCombo->setCurrentIndex(0);
    };

    clearBorder(m_outerBorder);
    clearBorder(m_innerBorder);
    clearBorder(m_headerBottomBorder);

    m_headerBgCheck->setChecked(false);
    m_headerFgCheck->setChecked(false);
    m_bodyBgCheck->setChecked(false);
    m_altRowCheck->setChecked(false);

    m_padTopSpin->setValue(3.0);
    m_padBottomSpin->setValue(3.0);
    m_padLeftSpin->setValue(4.0);
    m_padRightSpin->setValue(4.0);

    m_headerParaCombo->clear();
    m_bodyParaCombo->clear();

    blockAllSignals(false);
}
