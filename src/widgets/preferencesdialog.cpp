#include "preferencesdialog.h"
#include "prettyreadersettings.h"
#include "thememanager.h"
#include "hyphenator.h"

#include <KLocalizedString>
#include <KSyntaxHighlighting/Repository>
#include <KSyntaxHighlighting/Theme>

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QSpinBox>
#include <QVBoxLayout>

PrettyReaderConfigDialog::PrettyReaderConfigDialog(QWidget *parent,
                                                     ThemeManager *themeManager)
    : KConfigDialog(parent, QStringLiteral("settings"), PrettyReaderSettings::self())
    , m_themeManager(themeManager)
{
    // ===== General Page =====
    auto *generalPage = new QWidget;
    auto *generalLayout = new QVBoxLayout(generalPage);

    auto *themeGroup = new QGroupBox(i18n("Theme"));
    auto *themeGroupLayout = new QVBoxLayout(themeGroup);
    auto *themeRow = new QHBoxLayout;
    themeRow->addWidget(new QLabel(i18n("Default theme:")));
    m_themeCombo = new QComboBox;
    const QStringList themes = m_themeManager->availableThemes();
    for (const QString &id : themes)
        m_themeCombo->addItem(m_themeManager->themeName(id), id);
    themeRow->addWidget(m_themeCombo, 1);
    themeGroupLayout->addLayout(themeRow);
    generalLayout->addWidget(themeGroup);

    auto *filesGroup = new QGroupBox(i18n("Files"));
    auto *filesGroupLayout = new QVBoxLayout(filesGroup);

    auto *rememberCheck = new QCheckBox(i18n("Remember per-file settings"));
    rememberCheck->setObjectName(QStringLiteral("kcfg_RememberPerFileSettings"));
    filesGroupLayout->addWidget(rememberCheck);

    auto *expiryRow = new QHBoxLayout;
    expiryRow->addWidget(new QLabel(i18n("Expire metadata after:")));
    auto *expirySpin = new QSpinBox;
    expirySpin->setObjectName(QStringLiteral("kcfg_MetaInfoExpiryDays"));
    expirySpin->setRange(0, 3650);
    expirySpin->setSuffix(i18n(" days"));
    expiryRow->addWidget(expirySpin);
    expiryRow->addStretch();
    filesGroupLayout->addLayout(expiryRow);

    auto *autoReloadCheck = new QCheckBox(i18n("Auto-reload when file changes"));
    autoReloadCheck->setObjectName(QStringLiteral("kcfg_AutoReloadOnChange"));
    filesGroupLayout->addWidget(autoReloadCheck);

    generalLayout->addWidget(filesGroup);
    generalLayout->addStretch();

    addPage(generalPage, i18n("General"), QStringLiteral("preferences-other"));

    // ===== Display Page =====
    auto *displayPage = new QWidget;
    auto *displayLayout = new QVBoxLayout(displayPage);

    auto *defaultsGroup = new QGroupBox(i18n("Defaults"));
    auto *defaultsGroupLayout = new QVBoxLayout(defaultsGroup);

    auto *pageSizeRow = new QHBoxLayout;
    pageSizeRow->addWidget(new QLabel(i18n("Default page size:")));
    auto *pageSizeCombo = new QComboBox;
    pageSizeCombo->setObjectName(QStringLiteral("kcfg_DefaultPageSizeName"));
    pageSizeCombo->addItem(QStringLiteral("A4"));
    pageSizeCombo->addItem(QStringLiteral("Letter"));
    pageSizeCombo->addItem(QStringLiteral("A5"));
    pageSizeCombo->addItem(QStringLiteral("Legal"));
    pageSizeCombo->addItem(QStringLiteral("B5"));
    pageSizeRow->addWidget(pageSizeCombo);
    pageSizeRow->addStretch();
    defaultsGroupLayout->addLayout(pageSizeRow);

    auto *zoomRow = new QHBoxLayout;
    zoomRow->addWidget(new QLabel(i18n("Default zoom:")));
    auto *zoomSpin = new QDoubleSpinBox;
    zoomSpin->setObjectName(QStringLiteral("kcfg_DefaultZoom"));
    zoomSpin->setRange(0.25, 4.0);
    zoomSpin->setSuffix(QStringLiteral("x"));
    zoomSpin->setDecimals(2);
    zoomSpin->setSingleStep(0.25);
    zoomRow->addWidget(zoomSpin);
    zoomRow->addStretch();
    defaultsGroupLayout->addLayout(zoomRow);

    auto *viewModeRow = new QHBoxLayout;
    viewModeRow->addWidget(new QLabel(i18n("View mode:")));
    auto *viewModeCombo = new QComboBox;
    viewModeCombo->setObjectName(QStringLiteral("kcfg_ViewMode"));
    viewModeCombo->addItem(i18n("Continuous"));
    viewModeCombo->addItem(i18n("Single Page"));
    viewModeCombo->addItem(i18n("Facing Pages"));
    viewModeCombo->addItem(i18n("Facing Pages (First Alone)"));
    viewModeCombo->addItem(i18n("Continuous Facing"));
    viewModeCombo->addItem(i18n("Continuous Facing (First Alone)"));
    viewModeRow->addWidget(viewModeCombo);
    viewModeRow->addStretch();
    defaultsGroupLayout->addLayout(viewModeRow);

    displayLayout->addWidget(defaultsGroup);
    displayLayout->addStretch();

    addPage(displayPage, i18n("Display"), QStringLiteral("preferences-desktop-display"));

    // ===== Rendering Page =====
    auto *renderPage = new QWidget;
    auto *renderLayout = new QVBoxLayout(renderPage);

    auto *codeGroup = new QGroupBox(i18n("Code Blocks"));
    auto *codeGroupLayout = new QVBoxLayout(codeGroup);

    auto *syntaxCheck = new QCheckBox(i18n("Enable syntax highlighting"));
    syntaxCheck->setObjectName(QStringLiteral("kcfg_SyntaxHighlightingEnabled"));
    codeGroupLayout->addWidget(syntaxCheck);

    auto *highlightRow = new QHBoxLayout;
    highlightRow->addWidget(new QLabel(i18n("Highlight theme:")));
    auto *highlightCombo = new QComboBox;
    highlightCombo->setObjectName(QStringLiteral("kcfg_CodeHighlightTheme"));
    // Populate from KSyntaxHighlighting
    KSyntaxHighlighting::Repository repo;
    const auto kshThemes = repo.themes();
    highlightCombo->addItem(i18n("(Default)"), QString());
    for (const auto &t : kshThemes)
        highlightCombo->addItem(t.name(), t.name());
    highlightRow->addWidget(highlightCombo, 1);
    codeGroupLayout->addLayout(highlightRow);

    renderLayout->addWidget(codeGroup);

    auto *imagesGroup = new QGroupBox(i18n("Images"));
    auto *imagesGroupLayout = new QVBoxLayout(imagesGroup);
    auto *renderImagesCheck = new QCheckBox(i18n("Render images"));
    renderImagesCheck->setObjectName(QStringLiteral("kcfg_RenderImages"));
    imagesGroupLayout->addWidget(renderImagesCheck);
    renderLayout->addWidget(imagesGroup);

    auto *engineGroup = new QGroupBox(i18n("Rendering Engine"));
    auto *engineGroupLayout = new QVBoxLayout(engineGroup);
    auto *pdfRendererCheck = new QCheckBox(i18n("Use PDF renderer (HarfBuzz + Poppler)"));
    pdfRendererCheck->setObjectName(QStringLiteral("kcfg_UsePdfRenderer"));
    pdfRendererCheck->setToolTip(i18n("When enabled, uses a custom rendering pipeline with HarfBuzz text shaping and Poppler display. "
                                       "Provides proper OpenType features (old-style numerals, ligatures, etc.) and true WYSIWYG."));
    engineGroupLayout->addWidget(pdfRendererCheck);
    renderLayout->addWidget(engineGroup);

    renderLayout->addStretch();

    addPage(renderPage, i18n("Rendering"), QStringLiteral("preferences-desktop-theme"));

    // ===== Typography Page =====
    auto *typoPage = new QWidget;
    auto *typoLayout = new QVBoxLayout(typoPage);

    auto *hyphGroup = new QGroupBox(i18n("Hyphenation"));
    auto *hyphGroupLayout = new QVBoxLayout(hyphGroup);

    auto *hyphCheck = new QCheckBox(i18n("Enable hyphenation"));
    hyphCheck->setObjectName(QStringLiteral("kcfg_HyphenationEnabled"));
    hyphGroupLayout->addWidget(hyphCheck);

    auto *langRow = new QHBoxLayout;
    langRow->addWidget(new QLabel(i18n("Language:")));
    auto *langCombo = new QComboBox;
    langCombo->setObjectName(QStringLiteral("kcfg_HyphenationLanguage"));
    const QStringList langs = Hyphenator::availableLanguages();
    for (const QString &lang : langs)
        langCombo->addItem(lang, lang);
    langRow->addWidget(langCombo, 1);
    hyphGroupLayout->addLayout(langRow);

    auto *minWordRow = new QHBoxLayout;
    minWordRow->addWidget(new QLabel(i18n("Min word length:")));
    auto *minWordSpin = new QSpinBox;
    minWordSpin->setObjectName(QStringLiteral("kcfg_HyphenationMinWordLength"));
    minWordSpin->setRange(3, 20);
    minWordRow->addWidget(minWordSpin);
    minWordRow->addStretch();
    hyphGroupLayout->addLayout(minWordRow);

    typoLayout->addWidget(hyphGroup);

    auto *swGroup = new QGroupBox(i18n("Short Words"));
    auto *swGroupLayout = new QVBoxLayout(swGroup);
    auto *swCheck = new QCheckBox(i18n("Insert non-breaking spaces after short words"));
    swCheck->setObjectName(QStringLiteral("kcfg_ShortWordsEnabled"));
    swGroupLayout->addWidget(swCheck);
    typoLayout->addWidget(swGroup);

    typoLayout->addStretch();

    addPage(typoPage, i18n("Typography"), QStringLiteral("preferences-desktop-font"));

    connect(m_themeCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this]() { updateButtons(); });
}

void PrettyReaderConfigDialog::updateWidgets()
{
    // Load the theme combo from the saved setting
    QString savedTheme = PrettyReaderSettings::self()->defaultTheme();
    for (int i = 0; i < m_themeCombo->count(); ++i) {
        if (m_themeCombo->itemData(i).toString() == savedTheme) {
            m_themeCombo->setCurrentIndex(i);
            return;
        }
    }
    // If not found, select first
    if (m_themeCombo->count() > 0)
        m_themeCombo->setCurrentIndex(0);
}

void PrettyReaderConfigDialog::updateSettings()
{
    // Save the theme combo selection to the setting
    QString themeId = m_themeCombo->currentData().toString();
    PrettyReaderSettings::self()->setDefaultTheme(themeId);
    PrettyReaderSettings::self()->save();
}

bool PrettyReaderConfigDialog::hasChanged()
{
    QString savedTheme = PrettyReaderSettings::self()->defaultTheme();
    QString currentTheme = m_themeCombo->currentData().toString();
    return currentTheme != savedTheme;
}
