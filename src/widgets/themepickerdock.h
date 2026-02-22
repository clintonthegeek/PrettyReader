#ifndef PRETTYREADER_THEMEPICKERDOCK_H
#define PRETTYREADER_THEMEPICKERDOCK_H

#include <QWidget>
#include <functional>

class PaletteManager;
class PalettePickerWidget;
class StyleManager;
class ThemeComposer;
class ThemeManager;
class TypographyThemeManager;
class TypographyThemePickerWidget;
struct PageLayout;

class ThemePickerDock : public QWidget
{
    Q_OBJECT

public:
    explicit ThemePickerDock(ThemeManager *themeManager,
                             PaletteManager *paletteManager,
                             TypographyThemeManager *typographyThemeManager,
                             ThemeComposer *themeComposer,
                             QWidget *parent = nullptr);

    // Provider callbacks for data this dock doesn't own
    void setStyleManagerProvider(std::function<StyleManager *()> provider);
    void setPageLayoutProvider(std::function<PageLayout()> provider);

    // Sync picker highlights from composer state
    void syncPickersFromComposer();

    // Current selections (for save/restore)
    QString currentTypographyThemeId() const;
    QString currentColorSchemeId() const;
    void setCurrentTypographyThemeId(const QString &id);
    void setCurrentColorSchemeId(const QString &id);

Q_SIGNALS:
    void compositionApplied(); // typography or palette changed, compose() done

private Q_SLOTS:
    void onTypographyThemeSelected(const QString &id);
    void onPaletteSelected(const QString &id);
    void onCreatePalette();
    void onCreateTypographyTheme();

private:
    void buildUI();
    void composeAndNotify();

    ThemeManager *m_themeManager;
    PaletteManager *m_paletteManager;
    TypographyThemeManager *m_typographyThemeManager;
    ThemeComposer *m_themeComposer;
    std::function<StyleManager *()> m_styleManagerProvider;
    std::function<PageLayout()> m_pageLayoutProvider;

    // Pickers
    TypographyThemePickerWidget *m_typographyPicker = nullptr;
    PalettePickerWidget *m_palettePicker = nullptr;
};

#endif // PRETTYREADER_THEMEPICKERDOCK_H
