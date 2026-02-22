/*
 * paletteeditordialog.h — Editor dialog for color palettes
 *
 * Allows creating/editing a ColorPalette with KColorButton widgets
 * for each semantic color role, grouped logically.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef PRETTYREADER_PALETTEEDITORDIALOG_H
#define PRETTYREADER_PALETTEEDITORDIALOG_H

#include <QDialog>
#include <QHash>

class QDialogButtonBox;
class QLineEdit;
class QWidget;
class KColorButton;
class ColorPalette;

class PaletteEditorDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PaletteEditorDialog(QWidget *parent = nullptr);

    void setColorPalette(const ColorPalette &palette);
    ColorPalette colorPalette() const;

private:
    void buildUI();
    void updatePreviewStrip();

    QLineEdit *m_nameEdit = nullptr;
    QHash<QString, KColorButton *> m_colorButtons;
    QWidget *m_previewStrip = nullptr;
    QDialogButtonBox *m_buttonBox = nullptr;
};

#endif // PRETTYREADER_PALETTEEDITORDIALOG_H
