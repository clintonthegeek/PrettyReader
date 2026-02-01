#include "toolview.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>
#include <QVBoxLayout>

ToolView::ToolView(const QString &title, QWidget *content, QWidget *parent)
    : QFrame(parent)
    , m_content(content)
{
    setFrameShape(QFrame::NoFrame);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Title bar
    auto *titleBar = new QFrame;
    titleBar->setFrameShape(QFrame::StyledPanel);
    titleBar->setAutoFillBackground(true);
    auto *titleLayout = new QHBoxLayout(titleBar);
    titleLayout->setContentsMargins(6, 2, 2, 2);

    m_titleLabel = new QLabel(title);
    QFont boldFont = m_titleLabel->font();
    boldFont.setBold(true);
    boldFont.setPointSizeF(boldFont.pointSizeF() * 0.9);
    m_titleLabel->setFont(boldFont);
    titleLayout->addWidget(m_titleLabel);
    titleLayout->addStretch();

    auto *closeBtn = new QToolButton;
    closeBtn->setAutoRaise(true);
    closeBtn->setIcon(QIcon::fromTheme(QStringLiteral("window-close")));
    closeBtn->setToolTip(tr("Close panel"));
    closeBtn->setFixedSize(20, 20);
    titleLayout->addWidget(closeBtn);
    connect(closeBtn, &QToolButton::clicked, this, &ToolView::closeRequested);

    layout->addWidget(titleBar);

    // Content
    m_content->setParent(this);
    layout->addWidget(m_content, 1);
}

QString ToolView::title() const
{
    return m_titleLabel->text();
}
