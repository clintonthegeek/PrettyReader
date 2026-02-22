/*
 * typeseteditordialog.h — Editor dialog for type sets
 *
 * Allows creating/editing a TypeSet with TTF/OTF family and
 * Hershey fallback selection for Body, Heading, and Mono roles.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef PRETTYREADER_TYPESETEDITORDIALOG_H
#define PRETTYREADER_TYPESETEDITORDIALOG_H

#include <QDialog>

class QComboBox;
class QDialogButtonBox;
class QFontComboBox;
class QLabel;
class QLineEdit;
class TypeSet;

class TypeSetEditorDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TypeSetEditorDialog(QWidget *parent = nullptr);

    void setTypeSet(const TypeSet &typeSet);
    TypeSet typeSet() const;

private:
    void buildUI();
    void updatePreview(QLabel *preview, QFontComboBox *fontCombo);

    QLineEdit *m_nameEdit = nullptr;

    // Body role
    QFontComboBox *m_bodyFontCombo = nullptr;
    QComboBox *m_bodyHersheyCombo = nullptr;
    QLabel *m_bodyPreview = nullptr;

    // Heading role
    QFontComboBox *m_headingFontCombo = nullptr;
    QComboBox *m_headingHersheyCombo = nullptr;
    QLabel *m_headingPreview = nullptr;

    // Mono role
    QFontComboBox *m_monoFontCombo = nullptr;
    QComboBox *m_monoHersheyCombo = nullptr;
    QLabel *m_monoPreview = nullptr;

    QDialogButtonBox *m_buttonBox = nullptr;
};

#endif // PRETTYREADER_TYPESETEDITORDIALOG_H
