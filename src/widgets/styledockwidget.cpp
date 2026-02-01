#include "styledockwidget.h"
#include "characterstyle.h"
#include "stylemanager.h"
#include "thememanager.h"
#include "paragraphstyle.h"

#include <KColorButton>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFontComboBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMarginsF>
#include <QPageSize>
#include <QScrollArea>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>

StyleDockWidget::StyleDockWidget(ThemeManager *themeManager, QWidget *parent)
    : QWidget(parent)
    , m_themeManager(themeManager)
{
    buildUI();
}

void StyleDockWidget::buildUI()
{
    auto *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    auto *content = new QWidget;
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(12);

    // --- Theme section ---
    auto *themeLabel = new QLabel(tr("Theme"));
    themeLabel->setStyleSheet(QStringLiteral("font-weight: bold;"));
    layout->addWidget(themeLabel);

    m_themeCombo = new QComboBox;
    const QStringList themes = m_themeManager->availableThemes();
    for (const QString &id : themes) {
        m_themeCombo->addItem(m_themeManager->themeName(id), id);
    }
    layout->addWidget(m_themeCombo);

    connect(m_themeCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &StyleDockWidget::onThemeComboChanged);

    // --- Page Layout section ---
    auto *pageLabel = new QLabel(tr("Page Layout"));
    pageLabel->setStyleSheet(QStringLiteral("font-weight: bold;"));
    layout->addWidget(pageLabel);

    layout->addWidget(createPageLayoutSection());

    // --- Typography section ---
    auto *typoLabel = new QLabel(tr("Typography"));
    typoLabel->setStyleSheet(QStringLiteral("font-weight: bold;"));
    layout->addWidget(typoLabel);

    layout->addWidget(createTypographySection());

    // --- Spacing section ---
    auto *spacingLabel = new QLabel(tr("Spacing"));
    spacingLabel->setStyleSheet(QStringLiteral("font-weight: bold;"));
    layout->addWidget(spacingLabel);

    layout->addWidget(createSpacingSection());

    layout->addStretch();

    scrollArea->setWidget(content);

    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->addWidget(scrollArea);
}

QWidget *StyleDockWidget::createTypographySection()
{
    auto *container = new QWidget;
    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    // Body text group + foreground color
    auto *bodyGroup = createStyleGroup(tr("Body Text"),
                                       &m_bodyFontCombo, &m_bodySizeSpin,
                                       &m_bodyBoldBtn, &m_bodyItalicBtn);
    auto *bodyColorRow = new QHBoxLayout;
    bodyColorRow->addWidget(new QLabel(tr("Color:")));
    m_bodyFgColorBtn = new KColorButton;
    m_bodyFgColorBtn->setColor(QColor(0x1a, 0x1a, 0x1a));
    bodyColorRow->addWidget(m_bodyFgColorBtn);
    bodyColorRow->addStretch();
    qobject_cast<QVBoxLayout *>(bodyGroup->layout())->addLayout(bodyColorRow);
    connect(m_bodyFgColorBtn, &KColorButton::changed,
            this, &StyleDockWidget::onOverrideChanged);
    layout->addWidget(bodyGroup);

    // Headings group + foreground color
    auto *headingGroup = createStyleGroup(tr("Headings"),
                                          &m_headingFontCombo, &m_headingSizeSpin,
                                          &m_headingBoldBtn, &m_headingItalicBtn);
    auto *headingColorRow = new QHBoxLayout;
    headingColorRow->addWidget(new QLabel(tr("Color:")));
    m_headingFgColorBtn = new KColorButton;
    m_headingFgColorBtn->setColor(QColor(0x1a, 0x1a, 0x2e));
    headingColorRow->addWidget(m_headingFgColorBtn);
    headingColorRow->addStretch();
    qobject_cast<QVBoxLayout *>(headingGroup->layout())->addLayout(headingColorRow);
    connect(m_headingFgColorBtn, &KColorButton::changed,
            this, &StyleDockWidget::onOverrideChanged);
    layout->addWidget(headingGroup);

    // Code blocks group + foreground/background colors
    auto *codeGroup = createStyleGroup(tr("Code Blocks"),
                                       &m_codeFontCombo, &m_codeSizeSpin,
                                       &m_codeBoldBtn, &m_codeItalicBtn);
    auto *codeColorRow = new QHBoxLayout;
    codeColorRow->addWidget(new QLabel(tr("Fg:")));
    m_codeFgColorBtn = new KColorButton;
    m_codeFgColorBtn->setColor(QColor(0x1a, 0x1a, 0x1a));
    codeColorRow->addWidget(m_codeFgColorBtn);
    codeColorRow->addWidget(new QLabel(tr("Bg:")));
    m_codeBgColorBtn = new KColorButton;
    m_codeBgColorBtn->setColor(QColor(0xf6, 0xf8, 0xfa));
    codeColorRow->addWidget(m_codeBgColorBtn);
    codeColorRow->addStretch();
    qobject_cast<QVBoxLayout *>(codeGroup->layout())->addLayout(codeColorRow);
    connect(m_codeFgColorBtn, &KColorButton::changed,
            this, &StyleDockWidget::onOverrideChanged);
    connect(m_codeBgColorBtn, &KColorButton::changed,
            this, &StyleDockWidget::onOverrideChanged);
    layout->addWidget(codeGroup);

    // Link color (standalone row)
    auto *linkGroup = new QGroupBox(tr("Links"));
    auto *linkLayout = new QVBoxLayout(linkGroup);
    linkLayout->setContentsMargins(6, 6, 6, 6);
    auto *linkColorRow = new QHBoxLayout;
    linkColorRow->addWidget(new QLabel(tr("Color:")));
    m_linkFgColorBtn = new KColorButton;
    m_linkFgColorBtn->setColor(QColor(0x03, 0x66, 0xd6));
    linkColorRow->addWidget(m_linkFgColorBtn);
    linkColorRow->addStretch();
    linkLayout->addLayout(linkColorRow);
    connect(m_linkFgColorBtn, &KColorButton::changed,
            this, &StyleDockWidget::onOverrideChanged);
    layout->addWidget(linkGroup);

    // Set initial defaults
    m_bodyFontCombo->setCurrentFont(QFont(QStringLiteral("Noto Serif")));
    m_bodySizeSpin->setValue(11.0);

    m_headingFontCombo->setCurrentFont(QFont(QStringLiteral("Noto Sans")));
    m_headingSizeSpin->setValue(28.0);
    m_headingBoldBtn->setChecked(true);

    m_codeFontCombo->setCurrentFont(QFont(QStringLiteral("JetBrains Mono")));
    m_codeSizeSpin->setValue(10.0);

    return container;
}

QWidget *StyleDockWidget::createStyleGroup(const QString &label,
                                            QFontComboBox **fontCombo,
                                            QDoubleSpinBox **sizeSpin,
                                            QToolButton **boldBtn,
                                            QToolButton **italicBtn)
{
    auto *group = new QGroupBox(label);
    auto *layout = new QVBoxLayout(group);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(4);

    // Font family
    *fontCombo = new QFontComboBox;
    layout->addWidget(*fontCombo);

    // Size + Bold/Italic row
    auto *row = new QHBoxLayout;
    row->setSpacing(4);

    *sizeSpin = new QDoubleSpinBox;
    (*sizeSpin)->setRange(6.0, 72.0);
    (*sizeSpin)->setSuffix(tr("pt"));
    (*sizeSpin)->setDecimals(1);
    row->addWidget(*sizeSpin);

    *boldBtn = new QToolButton;
    (*boldBtn)->setText(tr("B"));
    (*boldBtn)->setCheckable(true);
    (*boldBtn)->setFont(QFont(QString(), -1, QFont::Bold));
    (*boldBtn)->setFixedSize(28, 28);
    row->addWidget(*boldBtn);

    *italicBtn = new QToolButton;
    (*italicBtn)->setText(tr("I"));
    (*italicBtn)->setCheckable(true);
    QFont italicFont;
    italicFont.setItalic(true);
    (*italicBtn)->setFont(italicFont);
    (*italicBtn)->setFixedSize(28, 28);
    row->addWidget(*italicBtn);

    layout->addLayout(row);

    // Connect signals for live preview
    connect(*fontCombo, &QFontComboBox::currentFontChanged,
            this, &StyleDockWidget::onOverrideChanged);
    connect(*sizeSpin, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, &StyleDockWidget::onOverrideChanged);
    connect(*boldBtn, &QToolButton::toggled,
            this, &StyleDockWidget::onOverrideChanged);
    connect(*italicBtn, &QToolButton::toggled,
            this, &StyleDockWidget::onOverrideChanged);

    return group;
}

QWidget *StyleDockWidget::createSpacingSection()
{
    auto *group = new QGroupBox(tr("Line & Paragraph"));
    auto *layout = new QVBoxLayout(group);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(4);

    // Line height
    auto *lhRow = new QHBoxLayout;
    lhRow->addWidget(new QLabel(tr("Line height:")));
    m_lineHeightSpin = new QSpinBox;
    m_lineHeightSpin->setRange(100, 250);
    m_lineHeightSpin->setSuffix(QStringLiteral("%"));
    m_lineHeightSpin->setValue(150);
    lhRow->addWidget(m_lineHeightSpin);
    layout->addLayout(lhRow);

    // First-line indent
    auto *indentRow = new QHBoxLayout;
    indentRow->addWidget(new QLabel(tr("First indent:")));
    m_firstLineIndentSpin = new QDoubleSpinBox;
    m_firstLineIndentSpin->setRange(0.0, 72.0);
    m_firstLineIndentSpin->setSuffix(QStringLiteral(" pt"));
    m_firstLineIndentSpin->setDecimals(1);
    m_firstLineIndentSpin->setValue(0.0);
    indentRow->addWidget(m_firstLineIndentSpin);
    layout->addLayout(indentRow);

    connect(m_lineHeightSpin, qOverload<int>(&QSpinBox::valueChanged),
            this, &StyleDockWidget::onOverrideChanged);
    connect(m_firstLineIndentSpin, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, &StyleDockWidget::onOverrideChanged);

    return group;
}

QWidget *StyleDockWidget::createPageLayoutSection()
{
    auto *group = new QGroupBox(tr("Page"));
    auto *layout = new QVBoxLayout(group);
    layout->setContentsMargins(6, 6, 6, 6);
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
    auto *marginsLabel = new QLabel(tr("Margins (mm):"));
    layout->addWidget(marginsLabel);

    auto makeMarginSpin = [this]() {
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

    // Connect signals
    connect(m_pageSizeCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &StyleDockWidget::onPageLayoutChanged);
    connect(m_orientationCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &StyleDockWidget::onPageLayoutChanged);
    connect(m_marginTopSpin, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, &StyleDockWidget::onPageLayoutChanged);
    connect(m_marginBottomSpin, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, &StyleDockWidget::onPageLayoutChanged);
    connect(m_marginLeftSpin, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, &StyleDockWidget::onPageLayoutChanged);
    connect(m_marginRightSpin, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, &StyleDockWidget::onPageLayoutChanged);

    return group;
}

PageLayout StyleDockWidget::currentPageLayout() const
{
    PageLayout pl;
    if (m_pageSizeCombo)
        pl.pageSizeId = static_cast<QPageSize::PageSizeId>(m_pageSizeCombo->currentData().toInt());
    if (m_orientationCombo)
        pl.orientation = static_cast<QPageLayout::Orientation>(m_orientationCombo->currentData().toInt());
    if (m_marginTopSpin)
        pl.margins = QMarginsF(m_marginLeftSpin->value(), m_marginTopSpin->value(),
                               m_marginRightSpin->value(), m_marginBottomSpin->value());
    return pl;
}

void StyleDockWidget::onPageLayoutChanged()
{
    Q_EMIT pageLayoutChanged();
}

QString StyleDockWidget::currentThemeId() const
{
    return m_themeCombo->currentData().toString();
}

void StyleDockWidget::setCurrentThemeId(const QString &id)
{
    for (int i = 0; i < m_themeCombo->count(); ++i) {
        if (m_themeCombo->itemData(i).toString() == id) {
            m_themeCombo->setCurrentIndex(i);
            return;
        }
    }
}

void StyleDockWidget::applyOverrides(StyleManager *sm)
{
    // Override BodyText
    ParagraphStyle *body = sm->paragraphStyle(QStringLiteral("BodyText"));
    if (body) {
        body->setFontFamily(m_bodyFontCombo->currentFont().family());
        body->setFontSize(m_bodySizeSpin->value());
        body->setFontWeight(m_bodyBoldBtn->isChecked() ? QFont::Bold : QFont::Normal);
        body->setFontItalic(m_bodyItalicBtn->isChecked());
        if (m_bodyFgColorBtn)
            body->setForeground(m_bodyFgColorBtn->color());
        if (m_lineHeightSpin)
            body->setLineHeightPercent(m_lineHeightSpin->value());
        if (m_firstLineIndentSpin && m_firstLineIndentSpin->value() > 0)
            body->setFirstLineIndent(m_firstLineIndentSpin->value());
    }

    // Override headings (H1-H6 share font family)
    QString headingFamily = m_headingFontCombo->currentFont().family();
    qreal headingBaseSize = m_headingSizeSpin->value();
    QFont::Weight headingWeight = m_headingBoldBtn->isChecked() ? QFont::Bold : QFont::Normal;
    bool headingItalic = m_headingItalicBtn->isChecked();

    static const qreal headingScales[] = {1.0, 0.857, 0.714, 0.571, 0.5, 0.429};
    for (int i = 1; i <= 6; ++i) {
        QString name = QStringLiteral("Heading%1").arg(i);
        ParagraphStyle *h = sm->paragraphStyle(name);
        if (h) {
            h->setFontFamily(headingFamily);
            h->setFontSize(headingBaseSize * headingScales[i - 1]);
            h->setFontWeight(headingWeight);
            h->setFontItalic(headingItalic);
            if (m_headingFgColorBtn)
                h->setForeground(m_headingFgColorBtn->color());
        }
    }

    // Override CodeBlock
    ParagraphStyle *code = sm->paragraphStyle(QStringLiteral("CodeBlock"));
    if (code) {
        code->setFontFamily(m_codeFontCombo->currentFont().family());
        code->setFontSize(m_codeSizeSpin->value());
        if (m_codeFgColorBtn)
            code->setForeground(m_codeFgColorBtn->color());
        if (m_codeBgColorBtn)
            code->setBackground(m_codeBgColorBtn->color());
    }

    // Override InlineCode character style
    CharacterStyle *inlineCode = sm->characterStyle(QStringLiteral("InlineCode"));
    if (inlineCode) {
        inlineCode->setFontFamily(m_codeFontCombo->currentFont().family());
        inlineCode->setFontSize(m_codeSizeSpin->value());
        if (m_codeFgColorBtn)
            inlineCode->setForeground(m_codeFgColorBtn->color());
        if (m_codeBgColorBtn)
            inlineCode->setBackground(m_codeBgColorBtn->color());
    }

    // Override Link character style
    CharacterStyle *link = sm->characterStyle(QStringLiteral("Link"));
    if (link && m_linkFgColorBtn)
        link->setForeground(m_linkFgColorBtn->color());
}

void StyleDockWidget::populateFromStyleManager(StyleManager *sm)
{
    // Block signals while populating to avoid triggering rebuilds
    const QSignalBlocker blocker(this);

    ParagraphStyle *body = sm->paragraphStyle(QStringLiteral("BodyText"));
    if (body) {
        if (body->hasExplicitForeground() && m_bodyFgColorBtn)
            m_bodyFgColorBtn->setColor(body->foreground());
    }

    // Use Heading1 as representative for heading colors
    ParagraphStyle *h1 = sm->paragraphStyle(QStringLiteral("Heading1"));
    if (h1 && h1->hasExplicitForeground() && m_headingFgColorBtn)
        m_headingFgColorBtn->setColor(h1->foreground());

    ParagraphStyle *codeBlock = sm->paragraphStyle(QStringLiteral("CodeBlock"));
    if (codeBlock) {
        if (codeBlock->hasExplicitForeground() && m_codeFgColorBtn)
            m_codeFgColorBtn->setColor(codeBlock->foreground());
        if (codeBlock->hasExplicitBackground() && m_codeBgColorBtn)
            m_codeBgColorBtn->setColor(codeBlock->background());
    }

    CharacterStyle *link = sm->characterStyle(QStringLiteral("Link"));
    if (link && m_linkFgColorBtn) {
        // CharacterStyle doesn't expose hasExplicitForeground yet, but link always has one
        QTextCharFormat cf;
        link->applyFormat(cf);
        if (cf.foreground().style() != Qt::NoBrush)
            m_linkFgColorBtn->setColor(cf.foreground().color());
    }
}

void StyleDockWidget::onThemeComboChanged(int index)
{
    Q_UNUSED(index);
    emit themeChanged(currentThemeId());
}

void StyleDockWidget::onOverrideChanged()
{
    emit styleOverrideChanged();
}
