#include "footnoteparser.h"

#include <QHash>
#include <QRegularExpression>

QString FootnoteParser::process(const QString &markdownText)
{
    m_rawDefs.clear();
    m_footnotes.clear();

    // Step 1: Extract all footnote definitions
    extractDefinitions(markdownText);

    if (m_rawDefs.isEmpty())
        return markdownText;

    // Step 2: Remove definitions from the text.
    // A definition starts with [^label]: at the beginning of a line
    // and continues with indented (4 spaces or 1 tab) continuation lines,
    // separated by blank lines that are followed by indented content.
    static const QRegularExpression defBlockRx(
        QStringLiteral(R"((?:^|\n)\[\^([^\]]+)\]:[ \t]+[^\n]*(?:\n(?:[ \t]+[^\n]*|\s*))*)")
    );

    QString cleaned = markdownText;

    // Remove definitions from bottom to top to preserve offsets
    QList<QPair<int, int>> removeRanges;
    auto it = defBlockRx.globalMatch(cleaned);
    while (it.hasNext()) {
        auto match = it.next();
        int start = match.capturedStart();
        int len = match.capturedLength();
        // Skip the leading newline if we captured one
        if (start > 0 && cleaned[start] == QLatin1Char('\n')) {
            start++;
            len--;
        }
        removeRanges.append({start, len});
    }

    // Remove in reverse order
    for (int i = removeRanges.size() - 1; i >= 0; --i) {
        cleaned.remove(removeRanges[i].first, removeRanges[i].second);
    }

    // Clean up excessive blank lines left by removal
    static const QRegularExpression excessBlanks(QStringLiteral(R"(\n{3,})"));
    cleaned.replace(excessBlanks, QStringLiteral("\n\n"));

    // Step 3: Order footnotes by first reference appearance
    orderByReference(markdownText);

    return cleaned;
}

void FootnoteParser::extractDefinitions(const QString &text)
{
    // Match footnote definition: [^label]: content
    // Continuation lines are indented by 4 spaces or a tab.
    // Multiple paragraphs are separated by blank lines followed by indented content.
    static const QRegularExpression defRx(
        QStringLiteral(R"(^\[\^([^\]]+)\]:[ \t]+(.+))"),
        QRegularExpression::MultilineOption);

    QStringList lines = text.split(QLatin1Char('\n'));
    int i = 0;
    int defOrder = 0;

    while (i < lines.size()) {
        QRegularExpressionMatch match = defRx.match(lines[i]);
        if (match.hasMatch()) {
            RawDefinition def;
            def.label = match.captured(1);
            def.content = match.captured(2).trimmed();
            def.sourceOrder = defOrder++;

            // Collect continuation lines (indented by 4 spaces or tab)
            ++i;
            bool inBlankRun = false;
            while (i < lines.size()) {
                const QString &line = lines[i];
                if (line.trimmed().isEmpty()) {
                    // Blank line - might be paragraph separator within footnote
                    inBlankRun = true;
                    ++i;
                    continue;
                }
                if (line.startsWith(QLatin1String("    "))
                    || line.startsWith(QLatin1Char('\t'))) {
                    // Continuation line
                    if (inBlankRun) {
                        def.content += QLatin1String("\n\n");
                        inBlankRun = false;
                    } else {
                        def.content += QLatin1Char(' ');
                    }
                    def.content += line.mid(line.startsWith(QLatin1Char('\t')) ? 1 : 4).trimmed();
                    ++i;
                } else {
                    // Not a continuation - end of this definition
                    break;
                }
            }

            m_rawDefs.append(def);
        } else {
            ++i;
        }
    }
}

void FootnoteParser::orderByReference(const QString &text)
{
    // Find all [^label] references in order of appearance
    static const QRegularExpression refRx(
        QStringLiteral(R"(\[\^([^\]]+)\])"));

    // Build a label -> raw definition map
    QHash<QString, RawDefinition> defMap;
    for (const auto &def : m_rawDefs)
        defMap.insert(def.label, def);

    // Scan references and assign sequential numbers
    QHash<QString, int> assignedNumbers;
    int nextNumber = 1;

    auto it = refRx.globalMatch(text);
    while (it.hasNext()) {
        auto match = it.next();
        QString label = match.captured(1);

        // Skip if this is actually a definition line (has ]: after it)
        int afterEnd = match.capturedEnd();
        if (afterEnd < text.length() && text[afterEnd] == QLatin1Char(':'))
            continue;

        if (!assignedNumbers.contains(label) && defMap.contains(label)) {
            assignedNumbers.insert(label, nextNumber++);
        }
    }

    // Build the ordered footnotes list
    m_footnotes.clear();
    m_footnotes.reserve(assignedNumbers.size());

    // Create a sorted list by assigned number
    QList<QPair<int, QString>> ordered;
    for (auto it = assignedNumbers.constBegin(); it != assignedNumbers.constEnd(); ++it)
        ordered.append({it.value(), it.key()});
    std::sort(ordered.begin(), ordered.end());

    for (const auto &pair : ordered) {
        const RawDefinition &raw = defMap.value(pair.second);
        FootnoteDefinition fn;
        fn.label = raw.label;
        fn.sequentialNumber = pair.first;
        fn.content = raw.content;
        m_footnotes.append(fn);
    }

    // Add any definitions that were never referenced (at the end)
    for (const auto &def : m_rawDefs) {
        if (!assignedNumbers.contains(def.label)) {
            FootnoteDefinition fn;
            fn.label = def.label;
            fn.sequentialNumber = nextNumber++;
            fn.content = def.content;
            m_footnotes.append(fn);
        }
    }
}
