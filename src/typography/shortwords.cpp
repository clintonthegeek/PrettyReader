#include "shortwords.h"

#include <QRegularExpression>

static constexpr QChar kNbsp(0x00A0);

ShortWords::ShortWords()
{
    loadEnglish();
    m_language = QStringLiteral("en");
}

void ShortWords::setLanguage(const QString &language)
{
    m_language = language;
    m_words.clear();

    QString lang = language.left(2).toLower();
    if (lang == QLatin1String("cs") || lang == QLatin1String("sk"))
        loadCzech();
    else if (lang == QLatin1String("pl"))
        loadPolish();
    else if (lang == QLatin1String("fr"))
        loadFrench();
    else if (lang == QLatin1String("de"))
        loadGerman();
    else
        loadEnglish();
}

QString ShortWords::process(const QString &text) const
{
    if (m_words.isEmpty() || text.isEmpty())
        return text;

    QString result;
    result.reserve(text.length());

    // State machine: scan for word boundaries and check if words
    // match the short-word list. Replace the following space with nbsp.
    int i = 0;
    int len = text.length();

    while (i < len) {
        // Skip non-letter characters
        if (!text[i].isLetter()) {
            result.append(text[i]);
            ++i;
            continue;
        }

        // Collect word
        int wordStart = i;
        while (i < len && text[i].isLetter())
            ++i;

        QString word = text.mid(wordStart, i - wordStart);

        // Check if this word is in the short-words list (case-insensitive)
        bool isShortWord = m_words.contains(word.toLower());

        result.append(word);

        // If it's a short word and followed by a single space and then
        // a letter (not end of text), replace the space with nbsp
        if (isShortWord && i < len && text[i] == QLatin1Char(' ')) {
            // Check that next char after space is a letter (not another space or punctuation)
            if (i + 1 < len && text[i + 1].isLetter()) {
                result.append(kNbsp);
                ++i; // skip the space
            }
            // Otherwise, keep the normal space
        }
    }

    return result;
}

void ShortWords::loadEnglish()
{
    // English prepositions, articles, conjunctions, and short common words
    // that should not appear alone at the end of a line.
    // Inspired by Scribus short-words plugin en.cfg
    static const char *words[] = {
        "a", "i",
        "an", "as", "at", "be", "by", "do", "go", "he", "if", "in",
        "is", "it", "me", "my", "no", "of", "on", "or", "so", "to",
        "up", "us", "we",
        "the", "and", "but", "for", "its", "nor", "not", "yet",
        "all", "are", "can", "did", "few", "got", "had", "has",
        "her", "him", "his", "how", "may", "our", "out", "own",
        "per", "she", "too", "two", "was", "who", "why",
    };

    for (const char *w : words)
        m_words.insert(QString::fromLatin1(w));
}

void ShortWords::loadCzech()
{
    // Czech/Slovak prepositions and conjunctions
    static const char *words[] = {
        "a", "i", "k", "o", "s", "u", "v", "z",
        "do", "ke", "ku", "na", "od", "po", "ve", "za", "ze",
        "se", "si", "to",
    };

    for (const char *w : words)
        m_words.insert(QString::fromLatin1(w));
}

void ShortWords::loadPolish()
{
    static const char *words[] = {
        "a", "i", "o", "u", "w", "z",
        "do", "ku", "na", "od", "po", "we", "za", "ze",
    };

    for (const char *w : words)
        m_words.insert(QString::fromLatin1(w));
}

void ShortWords::loadFrench()
{
    static const char *words[] = {
        "a", "y",
        "au", "ce", "de", "du", "en", "et", "il", "je", "la", "le",
        "ne", "ni", "on", "ou", "se", "si", "tu", "un",
        "les", "des", "une", "que", "qui", "par", "sur", "est",
    };

    for (const char *w : words)
        m_words.insert(QString::fromLatin1(w));
}

void ShortWords::loadGerman()
{
    static const char *words[] = {
        "am", "an", "da", "du", "er", "es", "im", "in", "ob",
        "so", "um", "zu",
        "als", "auf", "aus", "bei", "bis", "das", "dem", "den",
        "der", "des", "die", "ein", "hat", "ich", "ihr", "ist",
        "man", "mit", "nur", "und", "von", "vor", "wie", "wir",
    };

    for (const char *w : words)
        m_words.insert(QString::fromLatin1(w));
}
