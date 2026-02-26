#include "headerfooterdialog.h"
#include "droptargetlineedit.h"

#include <QApplication>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDrag>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMimeData>
#include <QMouseEvent>
#include <QVBoxLayout>

// --- DragTileLabel: a small draggable label for the tile palette ---

class DragTileLabel : public QLabel
{
public:
    DragTileLabel(const QString &displayText, const QString &insertText, QWidget *parent = nullptr)
        : QLabel(displayText, parent)
        , m_insertText(insertText)
    {
        setFrameStyle(QFrame::StyledPanel | QFrame::Raised);
        setMargin(6);
        setCursor(Qt::OpenHandCursor);
        setToolTip(insertText);
    }

protected:
    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton)
            m_dragStartPos = event->pos();
        QLabel::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        if (!(event->buttons() & Qt::LeftButton))
            return;
        if ((event->pos() - m_dragStartPos).manhattanLength()
            < QApplication::startDragDistance())
            return;

        auto *drag = new QDrag(this);
        auto *mimeData = new QMimeData;
        mimeData->setText(m_insertText);
        drag->setMimeData(mimeData);
        drag->exec(Qt::CopyAction);
    }

private:
    QString m_insertText;
    QPoint m_dragStartPos;
};

// --- HeaderFooterDialog ---

HeaderFooterDialog::HeaderFooterDialog(const PageLayout &layout, QWidget *parent)
    : QDialog(parent)
    , m_baseLayout(layout)
{
    setWindowTitle(tr("Edit Headers & Footers"));
    setMinimumWidth(600);

    auto *mainLayout = new QVBoxLayout(this);

    // Tile palette
    mainLayout->addWidget(createTilePalette());

    // Default header/footer section
    m_defaultSection = new QWidget;
    auto *defaultLayout = new QVBoxLayout(m_defaultSection);
    defaultLayout->setContentsMargins(0, 0, 0, 0);

    auto *headerGroup = new QGroupBox(tr("Header"));
    auto *headerLayout = new QVBoxLayout(headerGroup);
    headerLayout->addWidget(createFieldRow(m_headerLeftEdit, m_headerCenterEdit, m_headerRightEdit));
    defaultLayout->addWidget(headerGroup);

    auto *footerGroup = new QGroupBox(tr("Footer"));
    auto *footerLayout = new QVBoxLayout(footerGroup);
    footerLayout->addWidget(createFieldRow(m_footerLeftEdit, m_footerCenterEdit, m_footerRightEdit));
    defaultLayout->addWidget(footerGroup);

    mainLayout->addWidget(m_defaultSection);

    // Different first page
    m_differentFirstPage = new QCheckBox(tr("Different first page"));
    mainLayout->addWidget(m_differentFirstPage);

    m_firstPageSection = new QWidget;
    auto *firstLayout = new QVBoxLayout(m_firstPageSection);
    firstLayout->setContentsMargins(0, 0, 0, 0);

    auto *firstHeaderGroup = new QGroupBox(tr("First Page — Header"));
    auto *firstHeaderLayout = new QVBoxLayout(firstHeaderGroup);
    firstHeaderLayout->addWidget(createFieldRow(m_firstHeaderLeftEdit, m_firstHeaderCenterEdit, m_firstHeaderRightEdit));
    firstLayout->addWidget(firstHeaderGroup);

    auto *firstFooterGroup = new QGroupBox(tr("First Page — Footer"));
    auto *firstFooterLayout = new QVBoxLayout(firstFooterGroup);
    firstFooterLayout->addWidget(createFieldRow(m_firstFooterLeftEdit, m_firstFooterCenterEdit, m_firstFooterRightEdit));
    firstLayout->addWidget(firstFooterGroup);

    mainLayout->addWidget(m_firstPageSection);

    // Different odd/even pages
    m_differentOddEven = new QCheckBox(tr("Different odd and even pages"));
    mainLayout->addWidget(m_differentOddEven);

    m_oddEvenSection = new QWidget;
    auto *oddEvenLayout = new QVBoxLayout(m_oddEvenSection);
    oddEvenLayout->setContentsMargins(0, 0, 0, 0);

    auto *leftHeaderGroup = new QGroupBox(tr("Even Pages — Header"));
    auto *leftHeaderLayout = new QVBoxLayout(leftHeaderGroup);
    leftHeaderLayout->addWidget(createFieldRow(m_leftHeaderLeftEdit, m_leftHeaderCenterEdit, m_leftHeaderRightEdit));
    oddEvenLayout->addWidget(leftHeaderGroup);

    auto *leftFooterGroup = new QGroupBox(tr("Even Pages — Footer"));
    auto *leftFooterLayout = new QVBoxLayout(leftFooterGroup);
    leftFooterLayout->addWidget(createFieldRow(m_leftFooterLeftEdit, m_leftFooterCenterEdit, m_leftFooterRightEdit));
    oddEvenLayout->addWidget(leftFooterGroup);

    auto *rightHeaderGroup = new QGroupBox(tr("Odd Pages — Header"));
    auto *rightHeaderLayout = new QVBoxLayout(rightHeaderGroup);
    rightHeaderLayout->addWidget(createFieldRow(m_rightHeaderLeftEdit, m_rightHeaderCenterEdit, m_rightHeaderRightEdit));
    oddEvenLayout->addWidget(rightHeaderGroup);

    auto *rightFooterGroup = new QGroupBox(tr("Odd Pages — Footer"));
    auto *rightFooterLayout = new QVBoxLayout(rightFooterGroup);
    rightFooterLayout->addWidget(createFieldRow(m_rightFooterLeftEdit, m_rightFooterCenterEdit, m_rightFooterRightEdit));
    oddEvenLayout->addWidget(rightFooterGroup);

    mainLayout->addWidget(m_oddEvenSection);

    // Button box
    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttonBox);

    // Visibility connections
    connect(m_differentFirstPage, &QCheckBox::toggled, this, &HeaderFooterDialog::updateMasterPageVisibility);
    connect(m_differentOddEven, &QCheckBox::toggled, this, &HeaderFooterDialog::updateMasterPageVisibility);

    // Load initial values
    loadFromLayout(layout);
    updateMasterPageVisibility();
}

QWidget *HeaderFooterDialog::createTilePalette()
{
    auto *group = new QGroupBox(tr("Drag tiles into fields below"));
    auto *layout = new QHBoxLayout(group);
    layout->setSpacing(8);

    struct TileDef { QString label; QString insert; };
    const TileDef tiles[] = {
        {tr("Page X of Y"),  QStringLiteral("{page} / {pages}")},
        {tr("Page Number"),  QStringLiteral("{page}")},
        {tr("Title"),        QStringLiteral("{title}")},
        {tr("Filename"),     QStringLiteral("{filename}")},
        {tr("Date"),         QStringLiteral("{date}")},
        {tr("Full Date"),    QStringLiteral("{date:d MMMM yyyy}")},
    };

    for (const auto &tile : tiles) {
        layout->addWidget(new DragTileLabel(tile.label, tile.insert, group));
    }
    layout->addStretch();

    return group;
}

QWidget *HeaderFooterDialog::createFieldRow(DropTargetLineEdit *&leftEdit,
                                             DropTargetLineEdit *&centerEdit,
                                             DropTargetLineEdit *&rightEdit)
{
    auto *widget = new QWidget;
    auto *row = new QHBoxLayout(widget);
    row->setContentsMargins(0, 0, 0, 0);

    row->addWidget(new QLabel(tr("Left:")));
    leftEdit = new DropTargetLineEdit;
    leftEdit->setPlaceholderText(tr("Left"));
    row->addWidget(leftEdit, 1);

    row->addWidget(new QLabel(tr("Center:")));
    centerEdit = new DropTargetLineEdit;
    centerEdit->setPlaceholderText(tr("Center"));
    row->addWidget(centerEdit, 1);

    row->addWidget(new QLabel(tr("Right:")));
    rightEdit = new DropTargetLineEdit;
    rightEdit->setPlaceholderText(tr("Right"));
    row->addWidget(rightEdit, 1);

    return widget;
}

void HeaderFooterDialog::updateMasterPageVisibility()
{
    m_firstPageSection->setVisible(m_differentFirstPage->isChecked());
    m_oddEvenSection->setVisible(m_differentOddEven->isChecked());
}

void HeaderFooterDialog::loadFromLayout(const PageLayout &layout)
{
    // Default fields
    m_headerLeftEdit->setText(layout.headerLeft);
    m_headerCenterEdit->setText(layout.headerCenter);
    m_headerRightEdit->setText(layout.headerRight);
    m_footerLeftEdit->setText(layout.footerLeft);
    m_footerCenterEdit->setText(layout.footerCenter);
    m_footerRightEdit->setText(layout.footerRight);

    // Check if first page master exists
    bool hasFirst = layout.masterPages.contains(QStringLiteral("first"));
    m_differentFirstPage->setChecked(hasFirst);
    if (hasFirst) {
        const MasterPage &mp = layout.masterPages[QStringLiteral("first")];
        if (mp.hasHeaderLeft)   m_firstHeaderLeftEdit->setText(mp.headerLeft);
        if (mp.hasHeaderCenter) m_firstHeaderCenterEdit->setText(mp.headerCenter);
        if (mp.hasHeaderRight)  m_firstHeaderRightEdit->setText(mp.headerRight);
        if (mp.hasFooterLeft)   m_firstFooterLeftEdit->setText(mp.footerLeft);
        if (mp.hasFooterCenter) m_firstFooterCenterEdit->setText(mp.footerCenter);
        if (mp.hasFooterRight)  m_firstFooterRightEdit->setText(mp.footerRight);
    }

    // Check if left/right masters exist
    bool hasLeft = layout.masterPages.contains(QStringLiteral("left"));
    bool hasRight = layout.masterPages.contains(QStringLiteral("right"));
    m_differentOddEven->setChecked(hasLeft || hasRight);
    if (hasLeft) {
        const MasterPage &mp = layout.masterPages[QStringLiteral("left")];
        if (mp.hasHeaderLeft)   m_leftHeaderLeftEdit->setText(mp.headerLeft);
        if (mp.hasHeaderCenter) m_leftHeaderCenterEdit->setText(mp.headerCenter);
        if (mp.hasHeaderRight)  m_leftHeaderRightEdit->setText(mp.headerRight);
        if (mp.hasFooterLeft)   m_leftFooterLeftEdit->setText(mp.footerLeft);
        if (mp.hasFooterCenter) m_leftFooterCenterEdit->setText(mp.footerCenter);
        if (mp.hasFooterRight)  m_leftFooterRightEdit->setText(mp.footerRight);
    }
    if (hasRight) {
        const MasterPage &mp = layout.masterPages[QStringLiteral("right")];
        if (mp.hasHeaderLeft)   m_rightHeaderLeftEdit->setText(mp.headerLeft);
        if (mp.hasHeaderCenter) m_rightHeaderCenterEdit->setText(mp.headerCenter);
        if (mp.hasHeaderRight)  m_rightHeaderRightEdit->setText(mp.headerRight);
        if (mp.hasFooterLeft)   m_rightFooterLeftEdit->setText(mp.footerLeft);
        if (mp.hasFooterCenter) m_rightFooterCenterEdit->setText(mp.footerCenter);
        if (mp.hasFooterRight)  m_rightFooterRightEdit->setText(mp.footerRight);
    }
}

PageLayout HeaderFooterDialog::result() const
{
    PageLayout pl = m_baseLayout;

    // Write back default fields
    pl.headerLeft = m_headerLeftEdit->text();
    pl.headerCenter = m_headerCenterEdit->text();
    pl.headerRight = m_headerRightEdit->text();
    pl.footerLeft = m_footerLeftEdit->text();
    pl.footerCenter = m_footerCenterEdit->text();
    pl.footerRight = m_footerRightEdit->text();

    // Clear master pages we manage (preserve margin overrides from other sources)
    pl.masterPages.remove(QStringLiteral("first"));
    pl.masterPages.remove(QStringLiteral("left"));
    pl.masterPages.remove(QStringLiteral("right"));

    // First page
    if (m_differentFirstPage->isChecked()) {
        MasterPage mp;
        mp.name = QStringLiteral("first");

        auto setIfNonEmpty = [](const DropTargetLineEdit *edit, QString &field, bool &hasField) {
            if (!edit->text().isEmpty()) {
                field = edit->text();
                hasField = true;
            }
        };

        setIfNonEmpty(m_firstHeaderLeftEdit,   mp.headerLeft,   mp.hasHeaderLeft);
        setIfNonEmpty(m_firstHeaderCenterEdit,  mp.headerCenter, mp.hasHeaderCenter);
        setIfNonEmpty(m_firstHeaderRightEdit,   mp.headerRight,  mp.hasHeaderRight);
        setIfNonEmpty(m_firstFooterLeftEdit,    mp.footerLeft,   mp.hasFooterLeft);
        setIfNonEmpty(m_firstFooterCenterEdit,  mp.footerCenter, mp.hasFooterCenter);
        setIfNonEmpty(m_firstFooterRightEdit,   mp.footerRight,  mp.hasFooterRight);

        if (!mp.isDefault())
            pl.masterPages.insert(QStringLiteral("first"), mp);
    }

    // Odd/even pages
    if (m_differentOddEven->isChecked()) {
        auto buildMasterPage = [](const QString &name,
                                   const DropTargetLineEdit *hL, const DropTargetLineEdit *hC, const DropTargetLineEdit *hR,
                                   const DropTargetLineEdit *fL, const DropTargetLineEdit *fC, const DropTargetLineEdit *fR) {
            MasterPage mp;
            mp.name = name;
            auto setIfNonEmpty = [](const DropTargetLineEdit *edit, QString &field, bool &hasField) {
                if (!edit->text().isEmpty()) {
                    field = edit->text();
                    hasField = true;
                }
            };
            setIfNonEmpty(hL, mp.headerLeft,   mp.hasHeaderLeft);
            setIfNonEmpty(hC, mp.headerCenter, mp.hasHeaderCenter);
            setIfNonEmpty(hR, mp.headerRight,  mp.hasHeaderRight);
            setIfNonEmpty(fL, mp.footerLeft,   mp.hasFooterLeft);
            setIfNonEmpty(fC, mp.footerCenter, mp.hasFooterCenter);
            setIfNonEmpty(fR, mp.footerRight,  mp.hasFooterRight);
            return mp;
        };

        MasterPage leftMp = buildMasterPage(QStringLiteral("left"),
            m_leftHeaderLeftEdit, m_leftHeaderCenterEdit, m_leftHeaderRightEdit,
            m_leftFooterLeftEdit, m_leftFooterCenterEdit, m_leftFooterRightEdit);
        if (!leftMp.isDefault())
            pl.masterPages.insert(QStringLiteral("left"), leftMp);

        MasterPage rightMp = buildMasterPage(QStringLiteral("right"),
            m_rightHeaderLeftEdit, m_rightHeaderCenterEdit, m_rightHeaderRightEdit,
            m_rightFooterLeftEdit, m_rightFooterCenterEdit, m_rightFooterRightEdit);
        if (!rightMp.isDefault())
            pl.masterPages.insert(QStringLiteral("right"), rightMp);
    }

    return pl;
}
