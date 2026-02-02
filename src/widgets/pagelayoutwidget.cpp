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
#include <QSignalBlocker>
#include <QVBoxLayout>

PageLayoutWidget::PageLayoutWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(4);

    // Page type selector (master pages)
    auto *typeRow = new QHBoxLayout;
    typeRow->addWidget(new QLabel(tr("Page type:")));
    m_pageTypeCombo = new QComboBox;
    m_pageTypeCombo->addItem(tr("All Pages"));
    m_pageTypeCombo->addItem(tr("First Page"));
    m_pageTypeCombo->addItem(tr("Left Pages"));
    m_pageTypeCombo->addItem(tr("Right Pages"));
    typeRow->addWidget(m_pageTypeCombo);
    layout->addLayout(typeRow);

    // Page size (wrapped in a widget for show/hide)
    m_pageSizeRow = new QWidget;
    auto *sizeRowLayout = new QHBoxLayout(m_pageSizeRow);
    sizeRowLayout->setContentsMargins(0, 0, 0, 0);
    sizeRowLayout->addWidget(new QLabel(tr("Size:")));
    m_pageSizeCombo = new QComboBox;
    m_pageSizeCombo->addItem(QStringLiteral("A4"), static_cast<int>(QPageSize::A4));
    m_pageSizeCombo->addItem(QStringLiteral("Letter"), static_cast<int>(QPageSize::Letter));
    m_pageSizeCombo->addItem(QStringLiteral("A5"), static_cast<int>(QPageSize::A5));
    m_pageSizeCombo->addItem(QStringLiteral("Legal"), static_cast<int>(QPageSize::Legal));
    m_pageSizeCombo->addItem(QStringLiteral("B5"), static_cast<int>(QPageSize::B5));
    sizeRowLayout->addWidget(m_pageSizeCombo);
    layout->addWidget(m_pageSizeRow);

    // Orientation (wrapped in a widget for show/hide)
    m_orientationRow = new QWidget;
    auto *orientRowLayout = new QHBoxLayout(m_orientationRow);
    orientRowLayout->setContentsMargins(0, 0, 0, 0);
    orientRowLayout->addWidget(new QLabel(tr("Orientation:")));
    m_orientationCombo = new QComboBox;
    m_orientationCombo->addItem(tr("Portrait"), static_cast<int>(QPageLayout::Portrait));
    m_orientationCombo->addItem(tr("Landscape"), static_cast<int>(QPageLayout::Landscape));
    orientRowLayout->addWidget(m_orientationCombo);
    layout->addWidget(m_orientationRow);

    // Margins
    layout->addWidget(new QLabel(tr("Margins (mm):")));

    auto makeMarginSpin = []() {
        auto *spin = new QDoubleSpinBox;
        spin->setRange(-1.0, 50.0);
        spin->setSuffix(QStringLiteral(" mm"));
        spin->setDecimals(1);
        spin->setValue(25.0);
        spin->setSpecialValueText(tr("(inherit)"));
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

    auto makeFieldRow = [&](const QString &,
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

    // Page type combo
    connect(m_pageTypeCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &PageLayoutWidget::onPageTypeChanged);
}

void PageLayoutWidget::blockAllSignals(bool block)
{
    m_pageSizeCombo->blockSignals(block);
    m_orientationCombo->blockSignals(block);
    m_marginTopSpin->blockSignals(block);
    m_marginBottomSpin->blockSignals(block);
    m_marginLeftSpin->blockSignals(block);
    m_marginRightSpin->blockSignals(block);
    m_headerCheck->blockSignals(block);
    m_headerLeftEdit->blockSignals(block);
    m_headerCenterEdit->blockSignals(block);
    m_headerRightEdit->blockSignals(block);
    m_footerCheck->blockSignals(block);
    m_footerLeftEdit->blockSignals(block);
    m_footerCenterEdit->blockSignals(block);
    m_footerRightEdit->blockSignals(block);
    m_pageTypeCombo->blockSignals(block);
}

void PageLayoutWidget::onPageTypeChanged(int index)
{
    // Save current state before switching
    saveCurrentPageTypeState();

    // Determine new page type
    static const QString types[] = {QString(), QStringLiteral("first"),
                                     QStringLiteral("left"), QStringLiteral("right")};
    QString newType = (index >= 0 && index < 4) ? types[index] : QString();

    m_currentPageType = newType;
    loadPageTypeState(newType);

    Q_EMIT pageLayoutChanged();
}

void PageLayoutWidget::saveCurrentPageTypeState()
{
    if (m_currentPageType.isEmpty()) {
        // Saving base layout
        m_baseLayout.pageSizeId = static_cast<QPageSize::PageSizeId>(
            m_pageSizeCombo->currentData().toInt());
        m_baseLayout.orientation = static_cast<QPageLayout::Orientation>(
            m_orientationCombo->currentData().toInt());
        m_baseLayout.margins = QMarginsF(m_marginLeftSpin->value(),
                                          m_marginTopSpin->value(),
                                          m_marginRightSpin->value(),
                                          m_marginBottomSpin->value());
        m_baseLayout.headerEnabled = m_headerCheck->isChecked();
        m_baseLayout.headerLeft = m_headerLeftEdit->text();
        m_baseLayout.headerCenter = m_headerCenterEdit->text();
        m_baseLayout.headerRight = m_headerRightEdit->text();
        m_baseLayout.footerEnabled = m_footerCheck->isChecked();
        m_baseLayout.footerLeft = m_footerLeftEdit->text();
        m_baseLayout.footerCenter = m_footerCenterEdit->text();
        m_baseLayout.footerRight = m_footerRightEdit->text();
    } else {
        // Saving master page override
        MasterPage mp;
        mp.name = m_currentPageType;

        // Header/footer: tri-state
        Qt::CheckState hState = m_headerCheck->checkState();
        mp.headerEnabled = (hState == Qt::PartiallyChecked) ? -1 : (hState == Qt::Checked ? 1 : 0);
        Qt::CheckState fState = m_footerCheck->checkState();
        mp.footerEnabled = (fState == Qt::PartiallyChecked) ? -1 : (fState == Qt::Checked ? 1 : 0);

        // Margins: -1 means inherit
        mp.marginTop = m_marginTopSpin->value();
        mp.marginBottom = m_marginBottomSpin->value();
        mp.marginLeft = m_marginLeftSpin->value();
        mp.marginRight = m_marginRightSpin->value();

        // Header/footer text: empty means inherit (has* = false)
        if (!m_headerLeftEdit->text().isEmpty()) {
            mp.headerLeft = m_headerLeftEdit->text();
            mp.hasHeaderLeft = true;
        }
        if (!m_headerCenterEdit->text().isEmpty()) {
            mp.headerCenter = m_headerCenterEdit->text();
            mp.hasHeaderCenter = true;
        }
        if (!m_headerRightEdit->text().isEmpty()) {
            mp.headerRight = m_headerRightEdit->text();
            mp.hasHeaderRight = true;
        }
        if (!m_footerLeftEdit->text().isEmpty()) {
            mp.footerLeft = m_footerLeftEdit->text();
            mp.hasFooterLeft = true;
        }
        if (!m_footerCenterEdit->text().isEmpty()) {
            mp.footerCenter = m_footerCenterEdit->text();
            mp.hasFooterCenter = true;
        }
        if (!m_footerRightEdit->text().isEmpty()) {
            mp.footerRight = m_footerRightEdit->text();
            mp.hasFooterRight = true;
        }

        if (mp.isDefault())
            m_masterPages.remove(m_currentPageType);
        else
            m_masterPages.insert(m_currentPageType, mp);
    }
}

void PageLayoutWidget::loadPageTypeState(const QString &type)
{
    blockAllSignals(true);

    bool isBase = type.isEmpty();

    // Show/hide page size and orientation rows (only for base layout)
    m_pageSizeRow->setVisible(isBase);
    m_orientationRow->setVisible(isBase);

    if (isBase) {
        // Revert to two-state checkboxes
        m_headerCheck->setTristate(false);
        m_footerCheck->setTristate(false);

        // Load base layout values
        for (int i = 0; i < m_pageSizeCombo->count(); ++i) {
            if (m_pageSizeCombo->itemData(i).toInt() == static_cast<int>(m_baseLayout.pageSizeId)) {
                m_pageSizeCombo->setCurrentIndex(i);
                break;
            }
        }
        m_orientationCombo->setCurrentIndex(
            m_baseLayout.orientation == QPageLayout::Landscape ? 1 : 0);

        // Restore normal margin range
        m_marginTopSpin->setMinimum(5.0);
        m_marginBottomSpin->setMinimum(5.0);
        m_marginLeftSpin->setMinimum(5.0);
        m_marginRightSpin->setMinimum(5.0);

        m_marginTopSpin->setValue(m_baseLayout.margins.top());
        m_marginBottomSpin->setValue(m_baseLayout.margins.bottom());
        m_marginLeftSpin->setValue(m_baseLayout.margins.left());
        m_marginRightSpin->setValue(m_baseLayout.margins.right());

        m_headerCheck->setChecked(m_baseLayout.headerEnabled);
        m_headerLeftEdit->setText(m_baseLayout.headerLeft);
        m_headerCenterEdit->setText(m_baseLayout.headerCenter);
        m_headerRightEdit->setText(m_baseLayout.headerRight);
        m_headerLeftEdit->setPlaceholderText(tr("Left"));
        m_headerCenterEdit->setPlaceholderText(tr("Center"));
        m_headerRightEdit->setPlaceholderText(tr("Right"));

        m_footerCheck->setChecked(m_baseLayout.footerEnabled);
        m_footerLeftEdit->setText(m_baseLayout.footerLeft);
        m_footerCenterEdit->setText(m_baseLayout.footerCenter);
        m_footerRightEdit->setText(m_baseLayout.footerRight);
        m_footerLeftEdit->setPlaceholderText(tr("Left"));
        m_footerCenterEdit->setPlaceholderText(tr("Center"));
        m_footerRightEdit->setPlaceholderText(tr("Right"));
    } else {
        // Enable tri-state for master page checkboxes
        m_headerCheck->setTristate(true);
        m_footerCheck->setTristate(true);

        // Allow -1 for inherit
        m_marginTopSpin->setMinimum(-1.0);
        m_marginBottomSpin->setMinimum(-1.0);
        m_marginLeftSpin->setMinimum(-1.0);
        m_marginRightSpin->setMinimum(-1.0);

        MasterPage mp;
        auto it = m_masterPages.find(type);
        if (it != m_masterPages.end())
            mp = it.value();

        // Header/footer checkboxes: -1=partial(inherit), 0=unchecked, 1=checked
        if (mp.headerEnabled < 0)
            m_headerCheck->setCheckState(Qt::PartiallyChecked);
        else
            m_headerCheck->setCheckState(mp.headerEnabled ? Qt::Checked : Qt::Unchecked);

        if (mp.footerEnabled < 0)
            m_footerCheck->setCheckState(Qt::PartiallyChecked);
        else
            m_footerCheck->setCheckState(mp.footerEnabled ? Qt::Checked : Qt::Unchecked);

        // Margins: -1 = inherit
        m_marginTopSpin->setValue(mp.marginTop);
        m_marginBottomSpin->setValue(mp.marginBottom);
        m_marginLeftSpin->setValue(mp.marginLeft);
        m_marginRightSpin->setValue(mp.marginRight);

        // Header/footer text: show base layout as placeholder when not overridden
        m_headerLeftEdit->setText(mp.hasHeaderLeft ? mp.headerLeft : QString());
        m_headerLeftEdit->setPlaceholderText(m_baseLayout.headerLeft.isEmpty()
            ? tr("(inherit)") : m_baseLayout.headerLeft);
        m_headerCenterEdit->setText(mp.hasHeaderCenter ? mp.headerCenter : QString());
        m_headerCenterEdit->setPlaceholderText(m_baseLayout.headerCenter.isEmpty()
            ? tr("(inherit)") : m_baseLayout.headerCenter);
        m_headerRightEdit->setText(mp.hasHeaderRight ? mp.headerRight : QString());
        m_headerRightEdit->setPlaceholderText(m_baseLayout.headerRight.isEmpty()
            ? tr("(inherit)") : m_baseLayout.headerRight);

        m_footerLeftEdit->setText(mp.hasFooterLeft ? mp.footerLeft : QString());
        m_footerLeftEdit->setPlaceholderText(m_baseLayout.footerLeft.isEmpty()
            ? tr("(inherit)") : m_baseLayout.footerLeft);
        m_footerCenterEdit->setText(mp.hasFooterCenter ? mp.footerCenter : QString());
        m_footerCenterEdit->setPlaceholderText(m_baseLayout.footerCenter.isEmpty()
            ? tr("(inherit)") : m_baseLayout.footerCenter);
        m_footerRightEdit->setText(mp.hasFooterRight ? mp.footerRight : QString());
        m_footerRightEdit->setPlaceholderText(m_baseLayout.footerRight.isEmpty()
            ? tr("(inherit)") : m_baseLayout.footerRight);
    }

    // Update enabled state of header/footer fields
    bool hEnabled = m_headerCheck->checkState() != Qt::Unchecked;
    m_headerLeftEdit->setEnabled(hEnabled);
    m_headerCenterEdit->setEnabled(hEnabled);
    m_headerRightEdit->setEnabled(hEnabled);
    bool fEnabled = m_footerCheck->checkState() != Qt::Unchecked;
    m_footerLeftEdit->setEnabled(fEnabled);
    m_footerCenterEdit->setEnabled(fEnabled);
    m_footerRightEdit->setEnabled(fEnabled);

    blockAllSignals(false);
}

PageLayout PageLayoutWidget::currentPageLayout() const
{
    // Save current state first (const_cast needed since this is const)
    const_cast<PageLayoutWidget *>(this)->saveCurrentPageTypeState();

    PageLayout pl = m_baseLayout;
    pl.masterPages = m_masterPages;

    // If we're on the base page, read directly from widgets
    if (m_currentPageType.isEmpty()) {
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
    }

    return pl;
}

void PageLayoutWidget::setPageLayout(const PageLayout &layout)
{
    blockAllSignals(true);

    m_baseLayout = layout;
    m_masterPages = layout.masterPages;
    m_currentPageType.clear();

    // Reset combo to "All Pages"
    m_pageTypeCombo->setCurrentIndex(0);

    blockAllSignals(false);

    // Load base layout into controls
    loadPageTypeState(QString());
}
