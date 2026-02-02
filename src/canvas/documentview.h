#ifndef PRETTYREADER_DOCUMENTVIEW_H
#define PRETTYREADER_DOCUMENTVIEW_H

#include <QGraphicsView>
#include <QHash>
#include <QMarginsF>
#include <QSet>
#include <QString>
#include <QTextDocument>

#include "layoutengine.h"
#include "pagelayout.h"

class PageItem;
class PdfPageItem;
class RenderCache;

namespace Poppler { class Document; }

// A7: Cached link data per page
struct PageLinkInfo {
    QRectF rect;      // in page-local points
    QString url;
};

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

    // B1: Cursor mode
    enum CursorMode { HandTool, SelectionTool };
    Q_ENUM(CursorMode)

    void setCursorMode(CursorMode mode);
    CursorMode cursorMode() const { return m_cursorMode; }

    // B2: Selection
    void copySelection();
    void copySelectionAsRtf();
    void copySelectionAsMarkdown();
    void copySelectionAsComplexRtf();
    void clearSelection();
    bool hasSelection() const { return !m_pagesWithSelection.isEmpty(); }

    // Source breadcrumbs for markdown-faithful copy + styled RTF export
    void setSourceData(const QString &processedMarkdown,
                       const QList<Layout::SourceMapEntry> &sourceMap,
                       const Content::Document &contentDoc,
                       const QList<Layout::CodeBlockRegion> &codeBlockRegions = {});

    // Code block language overrides (per-session, persisted via MetadataStore)
    void setCodeBlockLanguageOverrides(const QHash<QString, QString> &overrides);
    QHash<QString, QString> codeBlockLanguageOverrides() const;
    void applyLanguageOverrides(Content::Document &doc) const;

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
    void statusHintChanged(const QString &hint);  // A7: hover hints
    void codeBlockLanguageChanged();

protected:
    void wheelEvent(QWheelEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void scrollContentsBy(int dx, int dy) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

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

    // B2: Text selection helpers
    void updateTextSelection();
    QString extractSelectedText() const;
    QString extractSelectedTextFromPdf() const; // Poppler fallback
    QList<Content::BlockNode> extractSelectedBlocks(int minLine, int maxLine) const;
    // Code block hit-test
    int codeBlockIndexAtScenePos(const QPointF &scenePos) const;

    // A7: Link hover helpers
    void checkLinkHover(const QPointF &scenePos);
    void ensureLinkCacheForPage(int pageNum);

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

    // B1: Cursor mode
    CursorMode m_cursorMode = HandTool;

    // B2: Text selection state (Okular-informed)
    bool m_textSelecting = false;
    QPointF m_selectPressPos;        // scene coords
    QPointF m_selectCurrentPos;      // scene coords
    QSet<int> m_pagesWithSelection;
    static constexpr int kSelectionThreshold = 5;  // 5px before selecting

    // A7: Link hover cache
    QHash<int, QList<PageLinkInfo>> m_linkCache;
    QString m_currentHoverLink;

    // Source breadcrumbs
    QString m_processedMarkdown;
    QList<Layout::SourceMapEntry> m_sourceMap;
    Content::Document m_contentDoc;
    QList<Layout::CodeBlockRegion> m_codeBlockRegions;
    QHash<QString, QString> m_codeBlockLanguageOverrides; // trimmed code -> language
    bool m_wordSelection = false; // set by double-click, cleared by mouse press

    static constexpr qreal kPageGap = 12.0;
    static constexpr qreal kSceneMargin = 40.0;
};

#endif // PRETTYREADER_DOCUMENTVIEW_H
