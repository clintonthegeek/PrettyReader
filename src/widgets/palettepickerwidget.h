// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef PRETTYREADER_PALETTEPICKERWIDGET_H
#define PRETTYREADER_PALETTEPICKERWIDGET_H

#include "resourcepickerwidget.h"

class PaletteManager;

class PalettePickerWidget : public ResourcePickerWidget
{
    Q_OBJECT

public:
    explicit PalettePickerWidget(PaletteManager *manager, QWidget *parent = nullptr);

protected:
    void populateGrid() override;

private:
    PaletteManager *m_manager = nullptr;
};

#endif // PRETTYREADER_PALETTEPICKERWIDGET_H
