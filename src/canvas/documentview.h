#ifndef PRETTYREADER_DOCUMENTVIEW_H
#define PRETTYREADER_DOCUMENTVIEW_H

#include <QGraphicsView>
#include <QMarginsF>
#include <QString>
#include <QTextDocument>

#include "pagelayout.h"

class PageItem;
class PdfPageItem;
class RenderCache;

namespace Poppler { class Document; }

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
    ~DocumentView() override;

    // Legacy QTextDocument path
    void setDocument(QTextDocument *doc);
    QTextDocument *document() const { return m_document; }

    // New PDF path
    void setPdfData(const QByteArray &pdf);

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

    // View modes (Okular pattern)
    enum ViewMode { Continuous, SinglePage, FacingPages, FacingPagesFirstAlone, ContinuousFacing, ContinuousFacingFirstAlone };
    Q_ENUM(ViewMode)

    void setViewMode(ViewMode mode);
    ViewMode viewMode() const { return m_viewMode; }

    // Legacy compatibility
    void setContinuousMode(bool continuous);
    bool isContinuous() const { return m_viewMode == Continuous || m_viewMode == ContinuousFacing || m_viewMode == ContinuousFacingFirstAlone; }

    bool isPdfMode() const { return m_pdfMode; }
    QByteArray pdfData() const { return m_pdfData; }

Q_SIGNALS:
    void zoomChanged(int percent);
    void currentPageChanged(int page);
    void viewModeChanged(ViewMode mode);

protected:
    void wheelEvent(QWheelEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void scrollContentsBy(int dx, int dy) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private Q_SLOTS:
    void onPixmapReady(int pageNumber);

private:
    void layoutPages();
    void layoutPagesContinuous();
    void layoutPagesSingle();
    void layoutPagesFacing();
    void layoutPagesContinuousFacing();
    void layoutPagesFacingFirstAlone();
    void layoutPagesContinuousFacingFirstAlone();
    void updateCurrentPage();
    void clearPdfPages();

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
    ViewMode m_viewMode = Continuous;
    bool m_skipAutoFit = false;

    // PDF rendering
    bool m_pdfMode = false;
    QByteArray m_pdfData;
    Poppler::Document *m_popplerDoc = nullptr;
    RenderCache *m_renderCache = nullptr;
    QList<PdfPageItem *> m_pdfPageItems;

    // Middle-mouse smooth zoom (Okular pattern)
    bool m_middleZooming = false;
    QPoint m_middleZoomOrigin;
    QPointF m_middleZoomSceneAnchor;
    int m_middleZoomStartPercent = 100;

    static constexpr qreal kPageGap = 12.0;
    static constexpr qreal kSceneMargin = 40.0;
};

#endif // PRETTYREADER_DOCUMENTVIEW_H
