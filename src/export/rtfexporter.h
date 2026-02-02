#ifndef PRETTYREADER_RTFEXPORTER_H
#define PRETTYREADER_RTFEXPORTER_H

#include <QByteArray>
#include <QColor>
#include <QList>
#include <QMap>
#include <QString>

class QTextDocument;
class QTextBlock;
class QTextCharFormat;
class QTextBlockFormat;
class QTextTable;

// Exports a QTextDocument to RTF format.
// Walks the document blocks and generates RTF with proper font table,
// color table, paragraph formatting, and character formatting.

class RtfExporter
{
public:
    RtfExporter();

    // Export the entire document to RTF
    QByteArray exportDocument(const QTextDocument *document);

    // Export document to a file. Returns true on success.
    bool exportToFile(const QTextDocument *document, const QString &filePath);

    // Export to clipboard as RTF (for "Copy as Styled Text")
    static void copyToClipboard(const QTextDocument *document);

private:
    // Build font table from all fonts used in the document
    void buildFontTable(const QTextDocument *document);

    // Build color table from all colors used in the document
    void buildColorTable(const QTextDocument *document);

    // Get font index in the font table (adds if new)
    int fontIndex(const QString &family);

    // Get color index in the color table (adds if new)
    int colorIndex(const QColor &color);

    // Write RTF header (font table, color table, document defaults)
    void writeHeader(QByteArray &out);

    // Write a text block
    void writeBlock(QByteArray &out, const QTextBlock &block);

    // Write character formatting commands
    void writeCharFormat(QByteArray &out, const QTextCharFormat &fmt);

    // Write paragraph formatting commands
    void writeParaFormat(QByteArray &out, const QTextBlockFormat &fmt);

    // Write a table
    void writeTable(QByteArray &out, const QTextTable *table);

    // Escape RTF special characters
    static QByteArray escapeText(const QString &text);

    // Convert points to twips (1 point = 20 twips)
    static int toTwips(qreal points) { return qRound(points * 20.0); }

    // Convert points to half-points (1 point = 2 half-points)
    static int toHalfPoints(qreal points) { return qRound(points * 2.0); }

    QList<QString> m_fontTable;
    QMap<QString, int> m_fontMap;
    QList<QColor> m_colorTable;
    QMap<QRgb, int> m_colorMap;
};

#endif // PRETTYREADER_RTFEXPORTER_H
