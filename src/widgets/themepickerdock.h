#ifndef PRETTYREADER_THEMEPICKERDOCK_H
#define PRETTYREADER_THEMEPICKERDOCK_H

#include <QWidget>

class PaletteManager;
class PalettePickerWidget;
class ThemeComposer;
class ThemeManager;
class TypographyThemeManager;
class TypographyThemePickerWidget;

class ThemePickerDock : public QWidget
{
    Q_OBJECT

public:
    explicit ThemePickerDock(ThemeManager *themeManager,
                             PaletteManager *paletteManager,
                             TypographyThemeManager *typographyThemeManager,
                             ThemeComposer *themeComposer,
                             QWidget *parent = nullptr);

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
    // Pickers
    TypographyThemePickerWidget *m_typographyPicker = nullptr;
    PalettePickerWidget *m_palettePicker = nullptr;
};

#endif // PRETTYREADER_THEMEPICKERDOCK_H
