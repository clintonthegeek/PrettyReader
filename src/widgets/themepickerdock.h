#ifndef PRETTYREADER_THEMEPICKERDOCK_H
#define PRETTYREADER_THEMEPICKERDOCK_H

#include <QWidget>
#include <functional>

class QComboBox;
class QPushButton;
class FontPairingManager;
class FontPairingPickerWidget;
class PaletteManager;
class PalettePickerWidget;
class StyleManager;
class ThemeComposer;
class ThemeManager;
struct PageLayout;

class ThemePickerDock : public QWidget
{
    Q_OBJECT

public:
    explicit ThemePickerDock(ThemeManager *themeManager,
                             PaletteManager *paletteManager,
                             FontPairingManager *pairingManager,
                             ThemeComposer *themeComposer,
                             QWidget *parent = nullptr);

    QString currentThemeId() const;
    void setCurrentThemeId(const QString &id);

    // Called after a theme loads to sync picker highlights from composer state
    void syncPickersFromComposer();

    // Provider callbacks for data this dock doesn't own
    void setStyleManagerProvider(std::function<StyleManager *()> provider);
    void setPageLayoutProvider(std::function<PageLayout()> provider);

signals:
    void themeChanged(const QString &themeId);
    void compositionApplied(); // palette or pairing changed, compose() done

private slots:
    void onThemeComboChanged(int index);
    void onPaletteSelected(const QString &id);
    void onPairingSelected(const QString &id);
    void onCreatePalette();
    void onCreatePairing();
    void onNewTheme();
    void onSaveTheme();
    void onDeleteTheme();
    void onThemesChanged();

private:
    void buildUI();
    void composeAndNotify();

    ThemeManager *m_themeManager;
    PaletteManager *m_paletteManager;
    FontPairingManager *m_pairingManager;
    ThemeComposer *m_themeComposer;
    std::function<StyleManager *()> m_styleManagerProvider;
    std::function<PageLayout()> m_pageLayoutProvider;

    // Theme section
    QComboBox *m_themeCombo = nullptr;
    QPushButton *m_newBtn = nullptr;
    QPushButton *m_saveBtn = nullptr;
    QPushButton *m_deleteBtn = nullptr;

    // Palette & font pairing pickers
    PalettePickerWidget *m_palettePicker = nullptr;
    FontPairingPickerWidget *m_pairingPicker = nullptr;
};

#endif // PRETTYREADER_THEMEPICKERDOCK_H
