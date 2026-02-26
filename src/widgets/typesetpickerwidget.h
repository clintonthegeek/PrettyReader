// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef PRETTYREADER_TYPESETPICKERWIDGET_H
#define PRETTYREADER_TYPESETPICKERWIDGET_H

#include "resourcepickerwidget.h"

class TypeSetManager;

class TypeSetPickerWidget : public ResourcePickerWidget
{
    Q_OBJECT

public:
    explicit TypeSetPickerWidget(TypeSetManager *manager, QWidget *parent = nullptr);

protected:
    int gridColumns() const override { return 2; }
    void populateGrid() override;

private:
    TypeSetManager *m_manager = nullptr;
};

#endif // PRETTYREADER_TYPESETPICKERWIDGET_H
