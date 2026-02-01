#ifndef PRETTYREADER_STYLEDOCKWIDGET_H
#define PRETTYREADER_STYLEDOCKWIDGET_H

#include <QWidget>

#include "pagelayout.h"

class KColorButton;
class QComboBox;
class QDoubleSpinBox;
class QFontComboBox;
class QSpinBox;
class QToolButton;
class QVBoxLayout;
class StyleManager;
class ThemeManager;

class StyleDockWidget : public QWidget
{
    Q_OBJECT

public:
    explicit StyleDockWidget(ThemeManager *themeManager,
                             QWidget *parent = nullptr);

    QString currentThemeId() const;
    void setCurrentThemeId(const QString &id);

    // Apply current dock settings to a style manager
    void applyOverrides(StyleManager *sm);

    // Get current page layout from dock controls
    PageLayout currentPageLayout() const;

    // Update controls from current style manager state
    void populateFromStyleManager(StyleManager *sm);

signals:
    void themeChanged(const QString &themeId);
    void styleOverrideChanged();
    void pageLayoutChanged();

private slots:
    void onThemeComboChanged(int index);
    void onOverrideChanged();
    void onPageLayoutChanged();

private:
    void buildUI();
    QWidget *createTypographySection();
    QWidget *createPageLayoutSection();
    QWidget *createSpacingSection();
    QWidget *createStyleGroup(const QString &label,
                              QFontComboBox **fontCombo,
                              QDoubleSpinBox **sizeSpin,
                              QToolButton **boldBtn,
                              QToolButton **italicBtn);

    ThemeManager *m_themeManager;

    // Theme selector
    QComboBox *m_themeCombo = nullptr;

    // Body text controls
    QFontComboBox *m_bodyFontCombo = nullptr;
    QDoubleSpinBox *m_bodySizeSpin = nullptr;
    QToolButton *m_bodyBoldBtn = nullptr;
    QToolButton *m_bodyItalicBtn = nullptr;

    // Heading controls
    QFontComboBox *m_headingFontCombo = nullptr;
    QDoubleSpinBox *m_headingSizeSpin = nullptr;
    QToolButton *m_headingBoldBtn = nullptr;
    QToolButton *m_headingItalicBtn = nullptr;

    // Code block controls
    QFontComboBox *m_codeFontCombo = nullptr;
    QDoubleSpinBox *m_codeSizeSpin = nullptr;
    QToolButton *m_codeBoldBtn = nullptr;
    QToolButton *m_codeItalicBtn = nullptr;

    // Spacing controls
    QSpinBox *m_lineHeightSpin = nullptr;
    QDoubleSpinBox *m_firstLineIndentSpin = nullptr;

    // Color controls
    KColorButton *m_bodyFgColorBtn = nullptr;
    KColorButton *m_headingFgColorBtn = nullptr;
    KColorButton *m_codeFgColorBtn = nullptr;
    KColorButton *m_codeBgColorBtn = nullptr;
    KColorButton *m_linkFgColorBtn = nullptr;

    // Page layout controls
    QComboBox *m_pageSizeCombo = nullptr;
    QComboBox *m_orientationCombo = nullptr;
    QDoubleSpinBox *m_marginTopSpin = nullptr;
    QDoubleSpinBox *m_marginBottomSpin = nullptr;
    QDoubleSpinBox *m_marginLeftSpin = nullptr;
    QDoubleSpinBox *m_marginRightSpin = nullptr;
};

#endif // PRETTYREADER_STYLEDOCKWIDGET_H
