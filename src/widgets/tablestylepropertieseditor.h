#ifndef PRETTYREADER_TABLESTYLEPROPERTIESEDITOR_H
#define PRETTYREADER_TABLESTYLEPROPERTIESEDITOR_H

#include "tablestyle.h"

#include <QWidget>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QSpinBox;
class KColorButton;

class TableStylePropertiesEditor : public QWidget
{
    Q_OBJECT

public:
    explicit TableStylePropertiesEditor(QWidget *parent = nullptr);

    void loadTableStyle(const TableStyle &style,
                        const QStringList &paraStyleNames);
    void applyToTableStyle(TableStyle &style) const;
    void clear();

signals:
    void propertyChanged();

private:
    void buildUI();
    void blockAllSignals(bool block);

    struct BorderRow {
        QDoubleSpinBox *widthSpin = nullptr;
        KColorButton *colorBtn = nullptr;
        QComboBox *styleCombo = nullptr;
    };
    BorderRow createBorderRow(const QString &label, QWidget *parent);

    // Borders
    BorderRow m_outerBorder;
    BorderRow m_innerBorder;
    BorderRow m_headerBottomBorder;

    // Colors
    QCheckBox *m_headerBgCheck = nullptr;
    KColorButton *m_headerBgBtn = nullptr;
    QCheckBox *m_headerFgCheck = nullptr;
    KColorButton *m_headerFgBtn = nullptr;
    QCheckBox *m_bodyBgCheck = nullptr;
    KColorButton *m_bodyBgBtn = nullptr;
    QCheckBox *m_altRowCheck = nullptr;
    KColorButton *m_altRowBtn = nullptr;
    QSpinBox *m_altFreqSpin = nullptr;

    // Cell padding
    QDoubleSpinBox *m_padTopSpin = nullptr;
    QDoubleSpinBox *m_padBottomSpin = nullptr;
    QDoubleSpinBox *m_padLeftSpin = nullptr;
    QDoubleSpinBox *m_padRightSpin = nullptr;

    // Paragraph styles
    QComboBox *m_headerParaCombo = nullptr;
    QComboBox *m_bodyParaCombo = nullptr;
};

#endif // PRETTYREADER_TABLESTYLEPROPERTIESEDITOR_H
