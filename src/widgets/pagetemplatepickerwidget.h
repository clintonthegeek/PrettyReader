// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef PRETTYREADER_PAGETEMPLATEPICKERWIDGET_H
#define PRETTYREADER_PAGETEMPLATEPICKERWIDGET_H

#include "resourcepickerwidget.h"

class PageTemplateManager;

class PageTemplatePickerWidget : public ResourcePickerWidget
{
    Q_OBJECT

public:
    explicit PageTemplatePickerWidget(PageTemplateManager *manager, QWidget *parent = nullptr);

protected:
    int gridColumns() const override { return 2; }
    void populateGrid() override;

private:
    PageTemplateManager *m_manager = nullptr;
};

#endif // PRETTYREADER_PAGETEMPLATEPICKERWIDGET_H
