/*
 * fontdegradationmap.cpp — TTF/OTF to Hershey font family mapping
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "fontdegradationmap.h"

#include <QHash>

// ---------------------------------------------------------------------------
// Static mapping table (built once on first use)
// ---------------------------------------------------------------------------

static const QHash<QString, QString> &knownMappings()
{
    // Case-folded (toLower) font family -> Hershey family
    static const QHash<QString, QString> map = {
        // Serif fonts -> "Hershey Serif"
        {QStringLiteral("noto serif"),           QStringLiteral("Hershey Serif")},
        {QStringLiteral("times new roman"),      QStringLiteral("Hershey Serif")},
        {QStringLiteral("georgia"),              QStringLiteral("Hershey Serif")},
        {QStringLiteral("pt serif"),             QStringLiteral("Hershey Serif")},
        {QStringLiteral("crimson text"),         QStringLiteral("Hershey Serif")},
        {QStringLiteral("eb garamond"),          QStringLiteral("Hershey Serif")},
        {QStringLiteral("libre baskerville"),    QStringLiteral("Hershey Serif")},
        {QStringLiteral("dejavu serif"),         QStringLiteral("Hershey Serif")},
        {QStringLiteral("liberation serif"),     QStringLiteral("Hershey Serif")},

        // Sans-serif fonts -> "Hershey Sans"
        {QStringLiteral("noto sans"),            QStringLiteral("Hershey Sans")},
        {QStringLiteral("arial"),                QStringLiteral("Hershey Sans")},
        {QStringLiteral("helvetica"),            QStringLiteral("Hershey Sans")},
        {QStringLiteral("liberation sans"),      QStringLiteral("Hershey Sans")},
        {QStringLiteral("inter"),                QStringLiteral("Hershey Sans")},
        {QStringLiteral("roboto"),               QStringLiteral("Hershey Sans")},
        {QStringLiteral("open sans"),            QStringLiteral("Hershey Sans")},
        {QStringLiteral("lato"),                 QStringLiteral("Hershey Sans")},
        {QStringLiteral("dejavu sans"),          QStringLiteral("Hershey Sans")},

        // Monospace fonts -> "Hershey Roman"
        {QStringLiteral("jetbrains mono"),       QStringLiteral("Hershey Roman")},
        {QStringLiteral("fira code"),            QStringLiteral("Hershey Roman")},
        {QStringLiteral("source code pro"),      QStringLiteral("Hershey Roman")},
        {QStringLiteral("courier new"),          QStringLiteral("Hershey Roman")},
        {QStringLiteral("inconsolata"),          QStringLiteral("Hershey Roman")},
        {QStringLiteral("ibm plex mono"),        QStringLiteral("Hershey Roman")},
        {QStringLiteral("dejavu sans mono"),     QStringLiteral("Hershey Roman")},
        {QStringLiteral("liberation mono"),      QStringLiteral("Hershey Roman")},

        // Script / handwriting fonts -> "Hershey Script"
        {QStringLiteral("comic sans ms"),        QStringLiteral("Hershey Script")},
        {QStringLiteral("pacifico"),             QStringLiteral("Hershey Script")},
        {QStringLiteral("dancing script"),       QStringLiteral("Hershey Script")},

        // Blackletter / fraktur fonts -> "Hershey Gothic English"
        {QStringLiteral("unifrakturcook"),       QStringLiteral("Hershey Gothic English")},
        {QStringLiteral("unifrakturmaguntia"),   QStringLiteral("Hershey Gothic English")},
        {QStringLiteral("old english text mt"),  QStringLiteral("Hershey Gothic English")},
    };
    return map;
}

// ---------------------------------------------------------------------------
// Generic classification fallback
// ---------------------------------------------------------------------------

static QString classifyByName(const QString &lowerFamily)
{
    // Monospace heuristics (check first — most specific)
    static const char *monoPatterns[] = {
        "mono", "code", "courier", "console", "consolas",
        "hack", "inconsolata", "menlo", "monaco", "terminal",
    };
    for (const char *p : monoPatterns) {
        if (lowerFamily.contains(QLatin1String(p)))
            return QStringLiteral("Hershey Roman");
    }

    // Script / handwriting heuristics
    if (lowerFamily.contains(QLatin1String("script"))
        || lowerFamily.contains(QLatin1String("handwrit"))
        || lowerFamily.contains(QLatin1String("cursive"))) {
        return QStringLiteral("Hershey Script");
    }

    // Blackletter / fraktur heuristics
    if (lowerFamily.contains(QLatin1String("fraktur"))
        || lowerFamily.contains(QLatin1String("blackletter"))
        || lowerFamily.contains(QLatin1String("gothic"))
        || lowerFamily.contains(QLatin1String("textur"))) {
        return QStringLiteral("Hershey Gothic English");
    }

    // Serif heuristics (check after more specific categories)
    if (lowerFamily.contains(QLatin1String("serif"))
        && !lowerFamily.contains(QLatin1String("sans"))) {
        return QStringLiteral("Hershey Serif");
    }

    // Sans-serif heuristics
    if (lowerFamily.contains(QLatin1String("sans"))
        || lowerFamily.contains(QLatin1String("grotesk"))
        || lowerFamily.contains(QLatin1String("grotesque"))) {
        return QStringLiteral("Hershey Sans");
    }

    // No match — return empty to signal "use global default"
    return {};
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

QString FontDegradationMap::hersheyFamilyFor(const QString &fontFamily)
{
    if (fontFamily.isEmpty())
        return QStringLiteral("Hershey Sans");

    // 1. Exact match in the known table (case-insensitive via toLower key)
    const QString key = fontFamily.toLower();
    const auto &map = knownMappings();
    auto it = map.constFind(key);
    if (it != map.constEnd())
        return it.value();

    // 2. Generic classification by name patterns (key is already lowercased)
    QString classified = classifyByName(key);
    if (!classified.isEmpty())
        return classified;

    // 3. Ultimate fallback — Hershey Sans is the most neutral
    return QStringLiteral("Hershey Sans");
}
