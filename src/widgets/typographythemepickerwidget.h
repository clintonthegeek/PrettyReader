// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef PRETTYREADER_TYPOGRAPHYTHEMEPICKERWIDGET_H
#define PRETTYREADER_TYPOGRAPHYTHEMEPICKERWIDGET_H

#include <QWidget>

class TypographyThemeManager;

class TypographyThemePickerWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TypographyThemePickerWidget(TypographyThemeManager *manager, QWidget *parent = nullptr);

    void setCurrentThemeId(const QString &id);
    void refresh();

Q_SIGNALS:
    void themeSelected(const QString &id);
    void createRequested();

private:
    void rebuildGrid();

    TypographyThemeManager *m_manager = nullptr;
    QString m_currentId;
    class QGridLayout *m_gridLayout = nullptr;
};

#endif // PRETTYREADER_TYPOGRAPHYTHEMEPICKERWIDGET_H
