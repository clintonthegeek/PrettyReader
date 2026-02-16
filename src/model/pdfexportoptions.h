/*
 * pdfexportoptions.h — Options struct for PDF export dialog
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef PRETTYREADER_PDFEXPORTOPTIONS_H
#define PRETTYREADER_PDFEXPORTOPTIONS_H

#include <QSet>
#include <QString>

struct PdfExportOptions {
    // General — metadata
    QString title;
    QString author;
    QString subject;
    QString keywords;           // comma-separated

    // General — text copy behavior
    enum TextCopyMode { PlainText, MarkdownSource, UnwrappedParagraphs };
    TextCopyMode textCopyMode = PlainText;

    // Content — section selection
    QSet<int> excludedHeadingIndices;   // indices into doc.blocks of unchecked headings
    bool sectionsModified = false;      // true if user changed any checkboxes

    // Content — page range
    QString pageRangeExpr;              // raw expression, empty = all pages
    bool pageRangeModified = false;     // true if user entered a range

    // Output — bookmarks
    bool includeBookmarks = true;
    int bookmarkMaxDepth = 6;           // 1–6

    // Output — viewer preferences
    enum InitialView { ViewerDefault, ShowBookmarks, ShowThumbnails };
    InitialView initialView = ShowBookmarks;

    enum PageLayout { SinglePage, Continuous, FacingPages, FacingPagesFirstAlone };
    PageLayout pageLayout = Continuous;
};

#endif // PRETTYREADER_PDFEXPORTOPTIONS_H
