// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef PRETTYREADER_PAGETEMPLATEPICKERWIDGET_H
#define PRETTYREADER_PAGETEMPLATEPICKERWIDGET_H

#include <QWidget>

class PageTemplateManager;

class PageTemplatePickerWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PageTemplatePickerWidget(PageTemplateManager *manager, QWidget *parent = nullptr);

    void setCurrentTemplateId(const QString &id);
    void refresh();

Q_SIGNALS:
    void templateSelected(const QString &id);

private:
    void rebuildGrid();

    PageTemplateManager *m_manager = nullptr;
    QString m_currentId;
    class QGridLayout *m_gridLayout = nullptr;
};

#endif // PRETTYREADER_PAGETEMPLATEPICKERWIDGET_H
