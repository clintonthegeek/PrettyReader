// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef PRETTYREADER_TYPESETPICKERWIDGET_H
#define PRETTYREADER_TYPESETPICKERWIDGET_H

#include <QWidget>

class TypeSetManager;

class TypeSetPickerWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TypeSetPickerWidget(TypeSetManager *manager, QWidget *parent = nullptr);

    void setCurrentTypeSetId(const QString &id);
    void refresh();

Q_SIGNALS:
    void typeSetSelected(const QString &id);

private:
    void rebuildGrid();

    TypeSetManager *m_manager = nullptr;
    QString m_currentId;
    class QGridLayout *m_gridLayout = nullptr;
};

#endif // PRETTYREADER_TYPESETPICKERWIDGET_H
