// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef PRETTYREADER_FONTPAIRINGPICKERWIDGET_H
#define PRETTYREADER_FONTPAIRINGPICKERWIDGET_H

#include <QWidget>

class FontPairingManager;

class FontPairingPickerWidget : public QWidget
{
    Q_OBJECT

public:
    explicit FontPairingPickerWidget(FontPairingManager *manager, QWidget *parent = nullptr);

    void setCurrentPairingId(const QString &id);
    void refresh();

Q_SIGNALS:
    void pairingSelected(const QString &id);
    void createRequested();

private:
    void rebuildGrid();

    FontPairingManager *m_manager = nullptr;
    QString m_currentId;
    class QGridLayout *m_gridLayout = nullptr;
};

#endif // PRETTYREADER_FONTPAIRINGPICKERWIDGET_H
