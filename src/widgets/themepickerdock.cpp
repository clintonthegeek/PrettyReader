#include "themepickerdock.h"
#include "palettepickerwidget.h"
#include "pagetemplatepickerwidget.h"
#include "typesetpickerwidget.h"
#include "colorpalette.h"
#include "palettemanager.h"
#include "pagetemplate.h"
#include "pagetemplatemanager.h"
#include "themecomposer.h"
#include "thememanager.h"
#include "typeset.h"
#include "typesetmanager.h"

#include <QVBoxLayout>

ThemePickerDock::ThemePickerDock(ThemeManager *themeManager,
                                 PaletteManager *paletteManager,
                                 TypeSetManager *typeSetManager,
                                 PageTemplateManager *pageTemplateManager,
                                 ThemeComposer *themeComposer,
                                 QWidget *parent)
    : QWidget(parent)
    , m_themeManager(themeManager)
    , m_paletteManager(paletteManager)
    , m_typeSetManager(typeSetManager)
    , m_pageTemplateManager(pageTemplateManager)
    , m_themeComposer(themeComposer)
{
    buildUI();
}

void ThemePickerDock::buildUI()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    // --- Type Set Picker ---
    m_typeSetPicker = new TypeSetPickerWidget(m_typeSetManager, this);
    layout->addWidget(m_typeSetPicker);

    connect(m_typeSetPicker, &TypeSetPickerWidget::resourceSelected,
            this, &ThemePickerDock::onTypeSetSelected);

    // --- Color Palette Picker ---
    m_palettePicker = new PalettePickerWidget(m_paletteManager, this);
    layout->addWidget(m_palettePicker);

    connect(m_palettePicker, &PalettePickerWidget::resourceSelected,
            this, &ThemePickerDock::onPaletteSelected);

    // --- Page Template Picker (initially hidden — visible in print mode) ---
    m_templateSection = new QWidget(this);
    auto *templateLayout = new QVBoxLayout(m_templateSection);
    templateLayout->setContentsMargins(0, 0, 0, 0);

    m_templatePicker = new PageTemplatePickerWidget(m_pageTemplateManager, m_templateSection);
    templateLayout->addWidget(m_templatePicker);

    connect(m_templatePicker, &PageTemplatePickerWidget::resourceSelected,
            this, &ThemePickerDock::onTemplateSelected);

    m_templateSection->setVisible(false);
    layout->addWidget(m_templateSection);

    layout->addStretch();
}

void ThemePickerDock::syncPickersFromComposer()
{
    if (m_palettePicker && !m_themeComposer->currentPalette().id.isEmpty())
        m_palettePicker->setCurrentId(m_themeComposer->currentPalette().id);
    if (m_typeSetPicker && !m_themeComposer->currentTypeSet().id.isEmpty())
        m_typeSetPicker->setCurrentId(m_themeComposer->currentTypeSet().id);
}

QString ThemePickerDock::currentTypeSetId() const
{
    if (m_themeComposer)
        return m_themeComposer->currentTypeSet().id;
    return {};
}

QString ThemePickerDock::currentColorSchemeId() const
{
    if (m_themeComposer)
        return m_themeComposer->currentPalette().id;
    return {};
}

QString ThemePickerDock::currentTemplateId() const
{
    return m_currentTemplateId;
}

void ThemePickerDock::setCurrentTypeSetId(const QString &id)
{
    if (m_typeSetPicker)
        m_typeSetPicker->setCurrentId(id);
}

void ThemePickerDock::setCurrentColorSchemeId(const QString &id)
{
    if (m_palettePicker)
        m_palettePicker->setCurrentId(id);
}

void ThemePickerDock::setCurrentTemplateId(const QString &id)
{
    m_currentTemplateId = id;
    if (m_templatePicker)
        m_templatePicker->setCurrentId(id);
}

void ThemePickerDock::setRenderMode(bool printMode)
{
    if (m_templateSection)
        m_templateSection->setVisible(printMode);
}

void ThemePickerDock::composeAndNotify()
{
    if (!m_themeComposer)
        return;
    Q_EMIT compositionApplied();
}

void ThemePickerDock::onTypeSetSelected(const QString &id)
{
    if (!m_typeSetManager || !m_themeComposer)
        return;
    TypeSet ts = m_typeSetManager->typeSet(id);
    m_themeComposer->setTypeSet(ts);
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

void ThemePickerDock::onTemplateSelected(const QString &id)
{
    if (!m_pageTemplateManager)
        return;
    m_currentTemplateId = id;
    PageTemplate tmpl = m_pageTemplateManager->pageTemplate(id);
    Q_EMIT templateApplied(tmpl.pageLayout);
}

