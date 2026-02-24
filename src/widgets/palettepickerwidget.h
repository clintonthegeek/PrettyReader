// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef PRETTYREADER_PALETTEPICKERWIDGET_H
#define PRETTYREADER_PALETTEPICKERWIDGET_H

#include <QWidget>

class PaletteManager;

class PalettePickerWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PalettePickerWidget(PaletteManager *manager, QWidget *parent = nullptr);

    void setCurrentPaletteId(const QString &id);
    void refresh();

Q_SIGNALS:
    void paletteSelected(const QString &id);

private:
    void rebuildGrid();

    PaletteManager *m_manager = nullptr;
    QString m_currentId;
    class QGridLayout *m_gridLayout = nullptr;
};

#endif // PRETTYREADER_PALETTEPICKERWIDGET_H
