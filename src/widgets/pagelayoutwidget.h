#ifndef PRETTYREADER_PAGELAYOUTWIDGET_H
#define PRETTYREADER_PAGELAYOUTWIDGET_H

#include <QHash>
#include <QWidget>

#include "pagelayout.h"

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLineEdit;

class PageLayoutWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PageLayoutWidget(QWidget *parent = nullptr);

    PageLayout currentPageLayout() const;
    void setPageLayout(const PageLayout &layout);

signals:
    void pageLayoutChanged();

private slots:
    void onPageTypeChanged(int index);

private:
    void saveCurrentPageTypeState();
    void loadPageTypeState(const QString &type);
    void blockAllSignals(bool block);

    QComboBox *m_pageTypeCombo = nullptr;
    QComboBox *m_pageSizeCombo = nullptr;
    QComboBox *m_orientationCombo = nullptr;
    QWidget *m_pageSizeRow = nullptr;
    QWidget *m_orientationRow = nullptr;
    QDoubleSpinBox *m_marginTopSpin = nullptr;
    QDoubleSpinBox *m_marginBottomSpin = nullptr;
    QDoubleSpinBox *m_marginLeftSpin = nullptr;
    QDoubleSpinBox *m_marginRightSpin = nullptr;

    // Header/footer controls
    QCheckBox *m_headerCheck = nullptr;
    QLineEdit *m_headerLeftEdit = nullptr;
    QLineEdit *m_headerCenterEdit = nullptr;
    QLineEdit *m_headerRightEdit = nullptr;
    QCheckBox *m_footerCheck = nullptr;
    QLineEdit *m_footerLeftEdit = nullptr;
    QLineEdit *m_footerCenterEdit = nullptr;
    QLineEdit *m_footerRightEdit = nullptr;

    // Master page state
    QHash<QString, MasterPage> m_masterPages;
    QString m_currentPageType;  // empty = "All Pages"
    PageLayout m_baseLayout;    // base layout for inherit reference
};

#endif // PRETTYREADER_PAGELAYOUTWIDGET_H
