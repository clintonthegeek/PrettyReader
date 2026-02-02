#ifndef PRETTYREADER_PREFERENCESDIALOG_H
#define PRETTYREADER_PREFERENCESDIALOG_H

#include <KConfigDialog>

class QComboBox;
class ThemeManager;

class PrettyReaderConfigDialog : public KConfigDialog
{
    Q_OBJECT

public:
    PrettyReaderConfigDialog(QWidget *parent, ThemeManager *themeManager);

protected:
    void updateWidgets() override;
    void updateSettings() override;
    bool hasChanged() override;

private:
    QComboBox *m_themeCombo;
    ThemeManager *m_themeManager;
};

#endif // PRETTYREADER_PREFERENCESDIALOG_H
