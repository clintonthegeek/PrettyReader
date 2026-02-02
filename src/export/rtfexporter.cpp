#include "rtfexporter.h"

#include <QApplication>
#include <QClipboard>
#include <QFile>
#include <QFont>
#include <QMimeData>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextDocument>
#include <QTextFragment>
#include <QTextFrame>
#include <QTextList>
#include <QTextTable>

// Helper to get font family from QTextCharFormat using non-deprecated API
static QString charFormatFontFamily(const QTextCharFormat &fmt)
{
    QStringList families = fmt.fontFamilies().toStringList();
    return families.isEmpty() ? QString() : families.first();
}

RtfExporter::RtfExporter() = default;

QByteArray RtfExporter::exportDocument(const QTextDocument *document)
{
    m_fontTable.clear();
    m_fontMap.clear();
    m_colorTable.clear();
    m_colorMap.clear();

    // Always have a default font and default colors
    fontIndex(document->defaultFont().family());
    colorIndex(QColor(Qt::black));
    colorIndex(QColor(Qt::white));

    // Pre-scan document to build font and color tables
    buildFontTable(document);
    buildColorTable(document);

    QByteArray rtf;
    rtf.reserve(document->characterCount() * 3); // rough estimate

    writeHeader(rtf);

    // Walk document blocks
    QTextBlock block = document->begin();
    QTextTable *currentTable = nullptr;

    while (block.isValid()) {
        // Check if block is inside a table
        QTextTable *table = qobject_cast<QTextTable *>(
            block.document()->objectForFormat(block.blockFormat()));

        // Detect table from cursor
        QTextCursor cursor(block);
        table = cursor.currentTable();

        if (table && table != currentTable) {
            // New table encountered
            writeTable(rtf, table);
            currentTable = table;
            // Skip all blocks that belong to this table
            QTextCursor endCursor = table->cellAt(table->rows() - 1,
                                                   table->columns() - 1).lastCursorPosition();
            block = endCursor.block();
            block = block.next();
            continue;
        }

        if (!table) {
            currentTable = nullptr;
            writeBlock(rtf, block);
        }

        block = block.next();
    }

    rtf.append("}");
    return rtf;
}

bool RtfExporter::exportToFile(const QTextDocument *document, const QString &filePath)
{
    QByteArray rtf = exportDocument(document);
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write(rtf);
    return true;
}

void RtfExporter::copyToClipboard(const QTextDocument *document)
{
    RtfExporter exporter;
    QByteArray rtf = exporter.exportDocument(document);

    auto *mimeData = new QMimeData;
    mimeData->setData(QStringLiteral("text/rtf"), rtf);
    mimeData->setData(QStringLiteral("application/rtf"), rtf);
    // Also set plain text as fallback
    mimeData->setText(document->toPlainText());

    QApplication::clipboard()->setMimeData(mimeData);
}

void RtfExporter::buildFontTable(const QTextDocument *document)
{
    QTextBlock block = document->begin();
    while (block.isValid()) {
        for (auto it = block.begin(); !it.atEnd(); ++it) {
            QTextFragment fragment = it.fragment();
            if (fragment.isValid()) {
                QTextCharFormat fmt = fragment.charFormat();
                if (!charFormatFontFamily(fmt).isEmpty())
                    fontIndex(charFormatFontFamily(fmt));
            }
        }
        block = block.next();
    }
}

void RtfExporter::buildColorTable(const QTextDocument *document)
{
    QTextBlock block = document->begin();
    while (block.isValid()) {
        for (auto it = block.begin(); !it.atEnd(); ++it) {
            QTextFragment fragment = it.fragment();
            if (fragment.isValid()) {
                QTextCharFormat fmt = fragment.charFormat();
                if (fmt.foreground().style() != Qt::NoBrush)
                    colorIndex(fmt.foreground().color());
                if (fmt.background().style() != Qt::NoBrush)
                    colorIndex(fmt.background().color());
            }
        }
        block = block.next();
    }
}

int RtfExporter::fontIndex(const QString &family)
{
    auto it = m_fontMap.find(family);
    if (it != m_fontMap.end())
        return it.value();

    int idx = m_fontTable.size();
    m_fontTable.append(family);
    m_fontMap.insert(family, idx);
    return idx;
}

int RtfExporter::colorIndex(const QColor &color)
{
    QRgb rgb = color.rgb();
    auto it = m_colorMap.find(rgb);
    if (it != m_colorMap.end())
        return it.value();

    int idx = m_colorTable.size();
    m_colorTable.append(color);
    m_colorMap.insert(rgb, idx);
    return idx;
}

void RtfExporter::writeHeader(QByteArray &out)
{
    // RTF header
    out.append("{\\rtf1\\ansi\\deff0\n");

    // Font table
    out.append("{\\fonttbl");
    for (int i = 0; i < m_fontTable.size(); ++i) {
        out.append("{\\f");
        out.append(QByteArray::number(i));

        // Determine font family type
        QFont testFont(m_fontTable[i]);
        switch (testFont.styleHint()) {
        case QFont::Serif:     out.append("\\froman "); break;
        case QFont::SansSerif: out.append("\\fswiss "); break;
        case QFont::Monospace: out.append("\\fmodern "); break;
        default:               out.append("\\fnil ");   break;
        }

        out.append(m_fontTable[i].toLatin1());
        out.append(";}");
    }
    out.append("}\n");

    // Color table
    out.append("{\\colortbl;");
    for (const QColor &c : m_colorTable) {
        out.append("\\red");
        out.append(QByteArray::number(c.red()));
        out.append("\\green");
        out.append(QByteArray::number(c.green()));
        out.append("\\blue");
        out.append(QByteArray::number(c.blue()));
        out.append(";");
    }
    out.append("}\n");

    // Document defaults
    out.append("\\viewkind4\\uc1\\pard\n");
}

void RtfExporter::writeParaFormat(QByteArray &out, const QTextBlockFormat &fmt)
{
    // Alignment
    Qt::Alignment align = fmt.alignment();
    if (align & Qt::AlignHCenter)
        out.append("\\qc");
    else if (align & Qt::AlignRight)
        out.append("\\qr");
    else if (align & Qt::AlignJustify)
        out.append("\\qj");
    else
        out.append("\\ql");

    // Spacing (in twips)
    if (fmt.topMargin() > 0) {
        out.append("\\sb");
        out.append(QByteArray::number(toTwips(fmt.topMargin())));
    }
    if (fmt.bottomMargin() > 0) {
        out.append("\\sa");
        out.append(QByteArray::number(toTwips(fmt.bottomMargin())));
    }

    // Indentation
    if (fmt.leftMargin() > 0) {
        out.append("\\li");
        out.append(QByteArray::number(toTwips(fmt.leftMargin())));
    }
    if (fmt.rightMargin() > 0) {
        out.append("\\ri");
        out.append(QByteArray::number(toTwips(fmt.rightMargin())));
    }
    if (fmt.textIndent() > 0) {
        out.append("\\fi");
        out.append(QByteArray::number(toTwips(fmt.textIndent())));
    }

    // Line spacing
    if (fmt.lineHeightType() == QTextBlockFormat::ProportionalHeight
        && fmt.lineHeight() > 100) {
        // RTF line spacing in twips; 240 twips = single spacing
        int spacing = qRound(240.0 * fmt.lineHeight() / 100.0);
        out.append("\\sl");
        out.append(QByteArray::number(spacing));
        out.append("\\slmult1");
    }

    out.append(" ");
}

void RtfExporter::writeCharFormat(QByteArray &out, const QTextCharFormat &fmt)
{
    // Font
    if (!charFormatFontFamily(fmt).isEmpty()) {
        out.append("\\f");
        out.append(QByteArray::number(fontIndex(charFormatFontFamily(fmt))));
    }

    // Font size in half-points
    if (fmt.fontPointSize() > 0) {
        out.append("\\fs");
        out.append(QByteArray::number(toHalfPoints(fmt.fontPointSize())));
    }

    // Bold
    if (fmt.fontWeight() >= QFont::Bold)
        out.append("\\b");
    else if (fmt.hasProperty(QTextFormat::FontWeight))
        out.append("\\b0");

    // Italic
    if (fmt.fontItalic())
        out.append("\\i");
    else if (fmt.hasProperty(QTextFormat::FontItalic))
        out.append("\\i0");

    // Underline
    if (fmt.fontUnderline())
        out.append("\\ul");
    else if (fmt.hasProperty(QTextFormat::FontUnderline))
        out.append("\\ulnone");

    // Strikethrough
    if (fmt.fontStrikeOut())
        out.append("\\strike");
    else if (fmt.hasProperty(QTextFormat::FontStrikeOut))
        out.append("\\strike0");

    // Superscript / subscript
    auto va = fmt.verticalAlignment();
    if (va == QTextCharFormat::AlignSuperScript)
        out.append("\\super");
    else if (va == QTextCharFormat::AlignSubScript)
        out.append("\\sub");

    // Foreground color
    if (fmt.foreground().style() != Qt::NoBrush) {
        out.append("\\cf");
        out.append(QByteArray::number(colorIndex(fmt.foreground().color()) + 1));
    }

    // Background color
    if (fmt.background().style() != Qt::NoBrush) {
        out.append("\\highlight");
        out.append(QByteArray::number(colorIndex(fmt.background().color()) + 1));
    }

    out.append(" ");
}

void RtfExporter::writeBlock(QByteArray &out, const QTextBlock &block)
{
    if (!block.isValid())
        return;

    out.append("\\pard");

    // Check for list
    QTextList *list = block.textList();
    if (list) {
        QTextListFormat listFmt = list->format();
        int indent = listFmt.indent();
        out.append("\\li");
        out.append(QByteArray::number(indent * 720)); // 720 twips = 0.5 inch

        switch (listFmt.style()) {
        case QTextListFormat::ListDecimal:
            out.append("{\\*\\pn\\pnlvlbody\\pndec}\\fi-360 ");
            break;
        case QTextListFormat::ListDisc:
            out.append("{\\*\\pn\\pnlvlblt\\pntxtb\\'B7}\\fi-360 ");
            break;
        case QTextListFormat::ListCircle:
            out.append("{\\*\\pn\\pnlvlblt\\pntxtb o}\\fi-360 ");
            break;
        case QTextListFormat::ListSquare:
            out.append("{\\*\\pn\\pnlvlblt\\pntxtb\\'A7}\\fi-360 ");
            break;
        default:
            out.append("\\fi-360 ");
            break;
        }
    }

    // Heading level
    int headingLevel = block.blockFormat().headingLevel();
    if (headingLevel > 0 && headingLevel <= 6) {
        out.append("\\s");
        out.append(QByteArray::number(headingLevel));
        out.append(" ");
    }

    writeParaFormat(out, block.blockFormat());

    // Write block fragments
    for (auto it = block.begin(); !it.atEnd(); ++it) {
        QTextFragment fragment = it.fragment();
        if (!fragment.isValid())
            continue;

        out.append("{");
        writeCharFormat(out, fragment.charFormat());
        out.append(escapeText(fragment.text()));
        out.append("}");
    }

    out.append("\\par\n");
}

void RtfExporter::writeTable(QByteArray &out, const QTextTable *table)
{
    int rows = table->rows();
    int cols = table->columns();

    for (int row = 0; row < rows; ++row) {
        // Row definition
        out.append("\\trowd\\trqc");

        // Column boundaries (approximate equal widths)
        int pageWidth = 9360; // ~6.5 inches in twips
        for (int col = 0; col < cols; ++col) {
            int right = pageWidth * (col + 1) / cols;
            out.append("\\cellx");
            out.append(QByteArray::number(right));
        }
        out.append("\n");

        // Cell contents
        for (int col = 0; col < cols; ++col) {
            QTextTableCell cell = table->cellAt(row, col);
            out.append("\\pard\\intbl ");

            // Write cell content (iterate blocks within the cell)
            QTextFrame::iterator cellIt = cell.begin();
            bool first = true;
            while (!cellIt.atEnd()) {
                QTextBlock cellBlock = cellIt.currentBlock();
                if (cellBlock.isValid()) {
                    if (!first)
                        out.append("\\par ");
                    first = false;

                    for (auto fragIt = cellBlock.begin(); !fragIt.atEnd(); ++fragIt) {
                        QTextFragment frag = fragIt.fragment();
                        if (frag.isValid()) {
                            out.append("{");
                            writeCharFormat(out, frag.charFormat());
                            out.append(escapeText(frag.text()));
                            out.append("}");
                        }
                    }
                }
                ++cellIt;
            }
            out.append("\\cell\n");
        }

        out.append("\\row\n");
    }
}

QByteArray RtfExporter::escapeText(const QString &text)
{
    QByteArray result;
    result.reserve(text.size() * 2);

    for (QChar ch : text) {
        ushort code = ch.unicode();
        if (code == '\\')
            result.append("\\\\");
        else if (code == '{')
            result.append("\\{");
        else if (code == '}')
            result.append("\\}");
        else if (code == '\t')
            result.append("\\tab ");
        else if (code == 0x00A0) // non-breaking space
            result.append("\\~");
        else if (code == 0x00AD) // soft hyphen
            result.append("\\-");
        else if (code == 0x2014) // em dash
            result.append("\\emdash ");
        else if (code == 0x2013) // en dash
            result.append("\\endash ");
        else if (code == 0x2018 || code == 0x2019) // smart quotes
            result.append(code == 0x2018 ? "\\lquote " : "\\rquote ");
        else if (code == 0x201C || code == 0x201D) // double smart quotes
            result.append(code == 0x201C ? "\\ldblquote " : "\\rdblquote ");
        else if (code > 127) {
            // Unicode character
            result.append("\\u");
            result.append(QByteArray::number(static_cast<qint16>(code)));
            result.append("?"); // fallback character
        } else {
            result.append(static_cast<char>(code));
        }
    }

    return result;
}
