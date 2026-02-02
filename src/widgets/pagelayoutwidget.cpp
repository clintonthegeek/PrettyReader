#include "pagelayoutwidget.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMarginsF>
#include <QPageSize>
#include <QVBoxLayout>

PageLayoutWidget::PageLayoutWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(4);

    // Page size
    auto *sizeRow = new QHBoxLayout;
    sizeRow->addWidget(new QLabel(tr("Size:")));
    m_pageSizeCombo = new QComboBox;
    m_pageSizeCombo->addItem(QStringLiteral("A4"), static_cast<int>(QPageSize::A4));
    m_pageSizeCombo->addItem(QStringLiteral("Letter"), static_cast<int>(QPageSize::Letter));
    m_pageSizeCombo->addItem(QStringLiteral("A5"), static_cast<int>(QPageSize::A5));
    m_pageSizeCombo->addItem(QStringLiteral("Legal"), static_cast<int>(QPageSize::Legal));
    m_pageSizeCombo->addItem(QStringLiteral("B5"), static_cast<int>(QPageSize::B5));
    sizeRow->addWidget(m_pageSizeCombo);
    layout->addLayout(sizeRow);

    // Orientation
    auto *orientRow = new QHBoxLayout;
    orientRow->addWidget(new QLabel(tr("Orientation:")));
    m_orientationCombo = new QComboBox;
    m_orientationCombo->addItem(tr("Portrait"), static_cast<int>(QPageLayout::Portrait));
    m_orientationCombo->addItem(tr("Landscape"), static_cast<int>(QPageLayout::Landscape));
    orientRow->addWidget(m_orientationCombo);
    layout->addLayout(orientRow);

    // Margins
    layout->addWidget(new QLabel(tr("Margins (mm):")));

    auto makeMarginSpin = []() {
        auto *spin = new QDoubleSpinBox;
        spin->setRange(5.0, 50.0);
        spin->setSuffix(QStringLiteral(" mm"));
        spin->setDecimals(1);
        spin->setValue(25.0);
        return spin;
    };

    auto *topBottomRow = new QHBoxLayout;
    topBottomRow->addWidget(new QLabel(tr("Top:")));
    m_marginTopSpin = makeMarginSpin();
    topBottomRow->addWidget(m_marginTopSpin);
    topBottomRow->addWidget(new QLabel(tr("Bottom:")));
    m_marginBottomSpin = makeMarginSpin();
    topBottomRow->addWidget(m_marginBottomSpin);
    layout->addLayout(topBottomRow);

    auto *leftRightRow = new QHBoxLayout;
    leftRightRow->addWidget(new QLabel(tr("Left:")));
    m_marginLeftSpin = makeMarginSpin();
    leftRightRow->addWidget(m_marginLeftSpin);
    leftRightRow->addWidget(new QLabel(tr("Right:")));
    m_marginRightSpin = makeMarginSpin();
    leftRightRow->addWidget(m_marginRightSpin);
    layout->addLayout(leftRightRow);

    // --- Header section ---
    m_headerCheck = new QCheckBox(tr("Header"));
    layout->addWidget(m_headerCheck);

    auto makeFieldRow = [&](const QString &leftLabel,
                            QLineEdit *&leftEdit,
                            QLineEdit *&centerEdit,
                            QLineEdit *&rightEdit) {
        auto *grid = new QHBoxLayout;
        grid->addWidget(new QLabel(tr("L:")));
        leftEdit = new QLineEdit;
        leftEdit->setPlaceholderText(tr("Left"));
        grid->addWidget(leftEdit);
        grid->addWidget(new QLabel(tr("C:")));
        centerEdit = new QLineEdit;
        centerEdit->setPlaceholderText(tr("Center"));
        grid->addWidget(centerEdit);
        grid->addWidget(new QLabel(tr("R:")));
        rightEdit = new QLineEdit;
        rightEdit->setPlaceholderText(tr("Right"));
        grid->addWidget(rightEdit);
        return grid;
    };

    auto *headerFieldRow = makeFieldRow(tr("Header"),
        m_headerLeftEdit, m_headerCenterEdit, m_headerRightEdit);
    layout->addLayout(headerFieldRow);

    // Header fields disabled when unchecked
    auto updateHeaderEnabled = [this](bool checked) {
        m_headerLeftEdit->setEnabled(checked);
        m_headerCenterEdit->setEnabled(checked);
        m_headerRightEdit->setEnabled(checked);
    };
    connect(m_headerCheck, &QCheckBox::toggled, this, updateHeaderEnabled);
    updateHeaderEnabled(false); // header off by default

    // --- Footer section ---
    m_footerCheck = new QCheckBox(tr("Footer"));
    m_footerCheck->setChecked(true);
    layout->addWidget(m_footerCheck);

    auto *footerFieldRow = makeFieldRow(tr("Footer"),
        m_footerLeftEdit, m_footerCenterEdit, m_footerRightEdit);
    layout->addLayout(footerFieldRow);

    // Footer fields disabled when unchecked
    auto updateFooterEnabled = [this](bool checked) {
        m_footerLeftEdit->setEnabled(checked);
        m_footerCenterEdit->setEnabled(checked);
        m_footerRightEdit->setEnabled(checked);
    };
    connect(m_footerCheck, &QCheckBox::toggled, this, updateFooterEnabled);
    updateFooterEnabled(true); // footer on by default

    // Placeholder hint
    auto *hint = new QLabel(tr("Placeholders: {page} {pages} {title} {filename} {date}"));
    hint->setWordWrap(true);
    QFont hintFont = hint->font();
    hintFont.setPointSizeF(hintFont.pointSizeF() * 0.85);
    hint->setFont(hintFont);
    hint->setStyleSheet(QStringLiteral("color: gray;"));
    layout->addWidget(hint);

    layout->addStretch();

    // Connect signals
    connect(m_pageSizeCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &PageLayoutWidget::pageLayoutChanged);
    connect(m_orientationCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &PageLayoutWidget::pageLayoutChanged);
    connect(m_marginTopSpin, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, &PageLayoutWidget::pageLayoutChanged);
    connect(m_marginBottomSpin, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, &PageLayoutWidget::pageLayoutChanged);
    connect(m_marginLeftSpin, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, &PageLayoutWidget::pageLayoutChanged);
    connect(m_marginRightSpin, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, &PageLayoutWidget::pageLayoutChanged);

    // Header/footer signal connections
    connect(m_headerCheck, &QCheckBox::toggled,
            this, &PageLayoutWidget::pageLayoutChanged);
    connect(m_headerLeftEdit, &QLineEdit::textChanged,
            this, &PageLayoutWidget::pageLayoutChanged);
    connect(m_headerCenterEdit, &QLineEdit::textChanged,
            this, &PageLayoutWidget::pageLayoutChanged);
    connect(m_headerRightEdit, &QLineEdit::textChanged,
            this, &PageLayoutWidget::pageLayoutChanged);
    connect(m_footerCheck, &QCheckBox::toggled,
            this, &PageLayoutWidget::pageLayoutChanged);
    connect(m_footerLeftEdit, &QLineEdit::textChanged,
            this, &PageLayoutWidget::pageLayoutChanged);
    connect(m_footerCenterEdit, &QLineEdit::textChanged,
            this, &PageLayoutWidget::pageLayoutChanged);
    connect(m_footerRightEdit, &QLineEdit::textChanged,
            this, &PageLayoutWidget::pageLayoutChanged);
}

PageLayout PageLayoutWidget::currentPageLayout() const
{
    PageLayout pl;
    pl.pageSizeId = static_cast<QPageSize::PageSizeId>(m_pageSizeCombo->currentData().toInt());
    pl.orientation = static_cast<QPageLayout::Orientation>(m_orientationCombo->currentData().toInt());
    pl.margins = QMarginsF(m_marginLeftSpin->value(), m_marginTopSpin->value(),
                            m_marginRightSpin->value(), m_marginBottomSpin->value());

    pl.headerEnabled = m_headerCheck->isChecked();
    pl.headerLeft = m_headerLeftEdit->text();
    pl.headerCenter = m_headerCenterEdit->text();
    pl.headerRight = m_headerRightEdit->text();
    pl.footerEnabled = m_footerCheck->isChecked();
    pl.footerLeft = m_footerLeftEdit->text();
    pl.footerCenter = m_footerCenterEdit->text();
    pl.footerRight = m_footerRightEdit->text();

    return pl;
}

void PageLayoutWidget::setPageLayout(const PageLayout &layout)
{
    const QSignalBlocker b1(m_pageSizeCombo);
    const QSignalBlocker b2(m_orientationCombo);
    const QSignalBlocker b3(m_marginTopSpin);
    const QSignalBlocker b4(m_marginBottomSpin);
    const QSignalBlocker b5(m_marginLeftSpin);
    const QSignalBlocker b6(m_marginRightSpin);
    const QSignalBlocker b7(m_headerCheck);
    const QSignalBlocker b8(m_headerLeftEdit);
    const QSignalBlocker b9(m_headerCenterEdit);
    const QSignalBlocker b10(m_headerRightEdit);
    const QSignalBlocker b11(m_footerCheck);
    const QSignalBlocker b12(m_footerLeftEdit);
    const QSignalBlocker b13(m_footerCenterEdit);
    const QSignalBlocker b14(m_footerRightEdit);

    for (int i = 0; i < m_pageSizeCombo->count(); ++i) {
        if (m_pageSizeCombo->itemData(i).toInt() == static_cast<int>(layout.pageSizeId)) {
            m_pageSizeCombo->setCurrentIndex(i);
            break;
        }
    }

    m_orientationCombo->setCurrentIndex(layout.orientation == QPageLayout::Landscape ? 1 : 0);
    m_marginTopSpin->setValue(layout.margins.top());
    m_marginBottomSpin->setValue(layout.margins.bottom());
    m_marginLeftSpin->setValue(layout.margins.left());
    m_marginRightSpin->setValue(layout.margins.right());

    m_headerCheck->setChecked(layout.headerEnabled);
    m_headerLeftEdit->setText(layout.headerLeft);
    m_headerCenterEdit->setText(layout.headerCenter);
    m_headerRightEdit->setText(layout.headerRight);
    m_headerLeftEdit->setEnabled(layout.headerEnabled);
    m_headerCenterEdit->setEnabled(layout.headerEnabled);
    m_headerRightEdit->setEnabled(layout.headerEnabled);

    m_footerCheck->setChecked(layout.footerEnabled);
    m_footerLeftEdit->setText(layout.footerLeft);
    m_footerCenterEdit->setText(layout.footerCenter);
    m_footerRightEdit->setText(layout.footerRight);
    m_footerLeftEdit->setEnabled(layout.footerEnabled);
    m_footerCenterEdit->setEnabled(layout.footerEnabled);
    m_footerRightEdit->setEnabled(layout.footerEnabled);
}
