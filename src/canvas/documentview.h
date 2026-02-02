#ifndef PRETTYREADER_DOCUMENTVIEW_H
#define PRETTYREADER_DOCUMENTVIEW_H

#include <QGraphicsView>
#include <QMarginsF>
#include <QString>
#include <QTextDocument>

#include "pagelayout.h"

class PageItem;

struct ViewState
{
    int zoomPercent = 100;
    int currentPage = 0;
    qreal scrollFraction = 0.0;
    bool valid = false;
};

class DocumentView : public QGraphicsView
{
    Q_OBJECT

public:
    explicit DocumentView(QWidget *parent = nullptr);

    void setDocument(QTextDocument *doc);
    QTextDocument *document() const { return m_document; }

    void setPageSize(const QSizeF &size);
    void setPageLayout(const PageLayout &layout);

    void setDocumentInfo(const QString &fileName, const QString &title);

    ViewState saveViewState() const;
    void restoreViewState(const ViewState &state);

    // Zoom
    void setZoomPercent(int percent);
    void zoomIn();
    void zoomOut();
    void fitWidth();
    void fitPage();
    int zoomPercent() const { return m_currentZoom; }

    // Navigation
    void goToPage(int page);
    void previousPage();
    void nextPage();
    int currentPage() const { return m_currentPage; }
    int pageCount() const { return m_pageCount; }

    // View mode
    void setContinuousMode(bool continuous);
    bool isContinuous() const { return m_continuousMode; }

Q_SIGNALS:
    void zoomChanged(int percent);
    void currentPageChanged(int page);

protected:
    void wheelEvent(QWheelEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void scrollContentsBy(int dx, int dy) override;

private:
    void layoutPages();
    void updateCurrentPage();

    QGraphicsScene *m_scene = nullptr;
    QTextDocument *m_document = nullptr;
    QList<PageItem *> m_pageItems;
    QSizeF m_pageSize;
    PageLayout m_pageLayout;
    QMarginsF m_marginsPoints;
    QString m_fileName;
    QString m_title;
    int m_pageCount = 0;
    int m_currentPage = 0;
    int m_currentZoom = 100;
    bool m_continuousMode = true;
    bool m_skipAutoFit = false;

    static constexpr qreal kPageGap = 12.0;
    static constexpr qreal kSceneMargin = 40.0;
};

#endif // PRETTYREADER_DOCUMENTVIEW_H
