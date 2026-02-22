#include "themepickerdock.h"
#include "fontpairingeditordialog.h"
#include "fontpairingpickerwidget.h"
#include "paletteeditordialog.h"
#include "palettepickerwidget.h"
#include "colorpalette.h"
#include "fontpairing.h"
#include "fontpairingmanager.h"
#include "pagelayout.h"
#include "palettemanager.h"
#include "stylemanager.h"
#include "themecomposer.h"
#include "thememanager.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

ThemePickerDock::ThemePickerDock(ThemeManager *themeManager,
                                 PaletteManager *paletteManager,
                                 FontPairingManager *pairingManager,
                                 ThemeComposer *themeComposer,
                                 QWidget *parent)
    : QWidget(parent)
    , m_themeManager(themeManager)
    , m_paletteManager(paletteManager)
    , m_pairingManager(pairingManager)
    , m_themeComposer(themeComposer)
{
    buildUI();

    connect(m_themeManager, &ThemeManager::themesChanged,
            this, &ThemePickerDock::onThemesChanged);
}

void ThemePickerDock::buildUI()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    // --- Theme section ---
    auto *themeLabel = new QLabel(tr("Theme"));
    themeLabel->setStyleSheet(QStringLiteral("font-weight: bold;"));
    layout->addWidget(themeLabel);

    auto *themeRow = new QHBoxLayout;
    m_themeCombo = new QComboBox;
    const QStringList themes = m_themeManager->availableThemes();
    for (const QString &id : themes) {
        m_themeCombo->addItem(m_themeManager->themeName(id), id);
    }
    themeRow->addWidget(m_themeCombo, 1);

    m_newBtn = new QPushButton(tr("New"));
    m_newBtn->setFixedWidth(50);
    themeRow->addWidget(m_newBtn);

    m_saveBtn = new QPushButton(tr("Save"));
    m_saveBtn->setFixedWidth(50);
    themeRow->addWidget(m_saveBtn);

    m_deleteBtn = new QPushButton(tr("Del"));
    m_deleteBtn->setFixedWidth(40);
    themeRow->addWidget(m_deleteBtn);

    layout->addLayout(themeRow);

    connect(m_themeCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &ThemePickerDock::onThemeComboChanged);
    connect(m_newBtn, &QPushButton::clicked, this, &ThemePickerDock::onNewTheme);
    connect(m_saveBtn, &QPushButton::clicked, this, &ThemePickerDock::onSaveTheme);
    connect(m_deleteBtn, &QPushButton::clicked, this, &ThemePickerDock::onDeleteTheme);

    // --- Color Palette Picker ---
    m_palettePicker = new PalettePickerWidget(m_paletteManager, this);
    layout->addWidget(m_palettePicker);

    connect(m_palettePicker, &PalettePickerWidget::paletteSelected,
            this, &ThemePickerDock::onPaletteSelected);

    // --- Font Pairing Picker ---
    m_pairingPicker = new FontPairingPickerWidget(m_pairingManager, this);
    layout->addWidget(m_pairingPicker);

    connect(m_pairingPicker, &FontPairingPickerWidget::pairingSelected,
            this, &ThemePickerDock::onPairingSelected);

    // --- [+] button: create new palette ---
    connect(m_palettePicker, &PalettePickerWidget::createRequested,
            this, &ThemePickerDock::onCreatePalette);

    // --- [+] button: create new font pairing ---
    connect(m_pairingPicker, &FontPairingPickerWidget::createRequested,
            this, &ThemePickerDock::onCreatePairing);

    layout->addStretch();
}

QString ThemePickerDock::currentThemeId() const
{
    return m_themeCombo->currentData().toString();
}

void ThemePickerDock::setCurrentThemeId(const QString &id)
{
    for (int i = 0; i < m_themeCombo->count(); ++i) {
        if (m_themeCombo->itemData(i).toString() == id) {
            m_themeCombo->setCurrentIndex(i);
            return;
        }
    }
}

void ThemePickerDock::syncPickersFromComposer()
{
    if (m_palettePicker && !m_themeComposer->currentPalette().id.isEmpty())
        m_palettePicker->setCurrentPaletteId(m_themeComposer->currentPalette().id);
    if (m_pairingPicker && !m_themeComposer->currentPairing().id.isEmpty())
        m_pairingPicker->setCurrentPairingId(m_themeComposer->currentPairing().id);
}

void ThemePickerDock::setStyleManagerProvider(std::function<StyleManager *()> provider)
{
    m_styleManagerProvider = std::move(provider);
}

void ThemePickerDock::setPageLayoutProvider(std::function<PageLayout()> provider)
{
    m_pageLayoutProvider = std::move(provider);
}

void ThemePickerDock::composeAndNotify()
{
    StyleManager *sm = m_styleManagerProvider ? m_styleManagerProvider() : nullptr;
    if (!sm || !m_themeComposer)
        return;
    m_themeComposer->compose(sm);
    Q_EMIT compositionApplied();
}

void ThemePickerDock::onThemeComboChanged(int index)
{
    Q_UNUSED(index);
    Q_EMIT themeChanged(currentThemeId());
}

void ThemePickerDock::onPaletteSelected(const QString &id)
{
    if (!m_paletteManager || !m_themeComposer)
        return;
    ColorPalette palette = m_paletteManager->palette(id);
    m_themeComposer->setColorPalette(palette);
    composeAndNotify();
}

void ThemePickerDock::onPairingSelected(const QString &id)
{
    if (!m_pairingManager || !m_themeComposer)
        return;
    FontPairing pairing = m_pairingManager->pairing(id);
    m_themeComposer->setFontPairing(pairing);
    composeAndNotify();
}

void ThemePickerDock::onCreatePalette()
{
    PaletteEditorDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        ColorPalette pal = dlg.colorPalette();
        QString id = m_paletteManager->savePalette(pal);
        m_palettePicker->setCurrentPaletteId(id);
        // Apply the new palette
        if (m_themeComposer) {
            m_themeComposer->setColorPalette(m_paletteManager->palette(id));
            composeAndNotify();
        }
    }
}

void ThemePickerDock::onCreatePairing()
{
    FontPairingEditorDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        FontPairing fp = dlg.fontPairing();
        QString id = m_pairingManager->savePairing(fp);
        m_pairingPicker->setCurrentPairingId(id);
        // Apply the new pairing
        if (m_themeComposer) {
            m_themeComposer->setFontPairing(m_pairingManager->pairing(id));
            composeAndNotify();
        }
    }
}

void ThemePickerDock::onNewTheme()
{
    bool ok;
    QString name = QInputDialog::getText(this, tr("New Theme"),
                                          tr("Theme name:"), QLineEdit::Normal,
                                          QString(), &ok);
    if (!ok || name.isEmpty())
        return;

    StyleManager *sm = m_styleManagerProvider ? m_styleManagerProvider() : nullptr;
    if (!sm)
        return;

    PageLayout pl = m_pageLayoutProvider ? m_pageLayoutProvider() : PageLayout{};
    QString id = m_themeManager->saveTheme(name, sm, pl);
    if (!id.isEmpty()) {
        QTimer::singleShot(0, this, [this, id]() {
            setCurrentThemeId(id);
        });
    }
}

void ThemePickerDock::onSaveTheme()
{
    StyleManager *sm = m_styleManagerProvider ? m_styleManagerProvider() : nullptr;
    if (!sm)
        return;

    QString themeId = currentThemeId();
    PageLayout pl = m_pageLayoutProvider ? m_pageLayoutProvider() : PageLayout{};
    if (m_themeManager->isBuiltinTheme(themeId)) {
        bool ok;
        QString name = QInputDialog::getText(this, tr("Save Theme As"),
                                              tr("New theme name:"), QLineEdit::Normal,
                                              m_themeManager->themeName(themeId) + tr(" (copy)"),
                                              &ok);
        if (!ok || name.isEmpty())
            return;
        QString id = m_themeManager->saveTheme(name, sm, pl);
        if (!id.isEmpty()) {
            QTimer::singleShot(0, this, [this, id]() {
                setCurrentThemeId(id);
            });
        }
    } else {
        m_themeManager->saveThemeAs(themeId, sm, pl);
    }
}

void ThemePickerDock::onDeleteTheme()
{
    QString themeId = currentThemeId();
    if (m_themeManager->isBuiltinTheme(themeId))
        return;

    int ret = QMessageBox::question(this, tr("Delete Theme"),
                                     tr("Delete theme \"%1\"?")
                                         .arg(m_themeManager->themeName(themeId)));
    if (ret == QMessageBox::Yes) {
        m_themeManager->deleteTheme(themeId);
    }
}

void ThemePickerDock::onThemesChanged()
{
    // Refresh theme combo
    const QSignalBlocker blocker(m_themeCombo);
    QString currentId = currentThemeId();
    m_themeCombo->clear();

    const QStringList themes = m_themeManager->availableThemes();
    for (const QString &id : themes) {
        m_themeCombo->addItem(m_themeManager->themeName(id), id);
    }

    // Restore selection
    setCurrentThemeId(currentId);
    if (m_themeCombo->currentIndex() < 0 && m_themeCombo->count() > 0)
        m_themeCombo->setCurrentIndex(0);

    m_deleteBtn->setEnabled(!m_themeManager->isBuiltinTheme(currentThemeId()));
}
