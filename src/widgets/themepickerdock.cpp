#include "themepickerdock.h"
#include "paletteeditordialog.h"
#include "palettepickerwidget.h"
#include "typographythemeeditordialog.h"
#include "typographythemepickerwidget.h"
#include "colorpalette.h"
#include "palettemanager.h"
#include "themecomposer.h"
#include "thememanager.h"
#include "typographytheme.h"
#include "typographythememanager.h"

#include <QVBoxLayout>

ThemePickerDock::ThemePickerDock(ThemeManager *themeManager,
                                 PaletteManager *paletteManager,
                                 TypographyThemeManager *typographyThemeManager,
                                 ThemeComposer *themeComposer,
                                 QWidget *parent)
    : QWidget(parent)
    , m_themeManager(themeManager)
    , m_paletteManager(paletteManager)
    , m_typographyThemeManager(typographyThemeManager)
    , m_themeComposer(themeComposer)
{
    buildUI();
}

void ThemePickerDock::buildUI()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    // --- Typography Theme Picker ---
    m_typographyPicker = new TypographyThemePickerWidget(m_typographyThemeManager, this);
    layout->addWidget(m_typographyPicker);

    connect(m_typographyPicker, &TypographyThemePickerWidget::themeSelected,
            this, &ThemePickerDock::onTypographyThemeSelected);
    connect(m_typographyPicker, &TypographyThemePickerWidget::createRequested,
            this, &ThemePickerDock::onCreateTypographyTheme);

    // --- Color Palette Picker ---
    m_palettePicker = new PalettePickerWidget(m_paletteManager, this);
    layout->addWidget(m_palettePicker);

    connect(m_palettePicker, &PalettePickerWidget::paletteSelected,
            this, &ThemePickerDock::onPaletteSelected);
    connect(m_palettePicker, &PalettePickerWidget::createRequested,
            this, &ThemePickerDock::onCreatePalette);

    layout->addStretch();
}

void ThemePickerDock::syncPickersFromComposer()
{
    if (m_palettePicker && !m_themeComposer->currentPalette().id.isEmpty())
        m_palettePicker->setCurrentPaletteId(m_themeComposer->currentPalette().id);
    if (m_typographyPicker && !m_themeComposer->currentTypographyTheme().id.isEmpty())
        m_typographyPicker->setCurrentThemeId(m_themeComposer->currentTypographyTheme().id);
}

QString ThemePickerDock::currentTypographyThemeId() const
{
    if (m_themeComposer)
        return m_themeComposer->currentTypographyTheme().id;
    return {};
}

QString ThemePickerDock::currentColorSchemeId() const
{
    if (m_themeComposer)
        return m_themeComposer->currentPalette().id;
    return {};
}

void ThemePickerDock::setCurrentTypographyThemeId(const QString &id)
{
    if (m_typographyPicker)
        m_typographyPicker->setCurrentThemeId(id);
}

void ThemePickerDock::setCurrentColorSchemeId(const QString &id)
{
    if (m_palettePicker)
        m_palettePicker->setCurrentPaletteId(id);
}

void ThemePickerDock::composeAndNotify()
{
    if (!m_themeComposer)
        return;
    Q_EMIT compositionApplied();
}

void ThemePickerDock::onTypographyThemeSelected(const QString &id)
{
    if (!m_typographyThemeManager || !m_themeComposer)
        return;
    TypographyTheme theme = m_typographyThemeManager->theme(id);
    m_themeComposer->setTypographyTheme(theme);
    composeAndNotify();
}

void ThemePickerDock::onPaletteSelected(const QString &id)
{
    if (!m_paletteManager || !m_themeComposer)
        return;
    ColorPalette palette = m_paletteManager->palette(id);
    m_themeComposer->setColorPalette(palette);
    composeAndNotify();
}

void ThemePickerDock::onCreatePalette()
{
    PaletteEditorDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        ColorPalette pal = dlg.colorPalette();
        QString id = m_paletteManager->savePalette(pal);
        m_palettePicker->setCurrentPaletteId(id);
        if (m_themeComposer) {
            m_themeComposer->setColorPalette(m_paletteManager->palette(id));
            composeAndNotify();
        }
    }
}

void ThemePickerDock::onCreateTypographyTheme()
{
    TypographyThemeEditorDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        TypographyTheme theme = dlg.typographyTheme();
        QString id = m_typographyThemeManager->saveTheme(theme);
        m_typographyPicker->setCurrentThemeId(id);
        if (m_themeComposer) {
            m_themeComposer->setTypographyTheme(m_typographyThemeManager->theme(id));
            composeAndNotify();
        }
    }
}
