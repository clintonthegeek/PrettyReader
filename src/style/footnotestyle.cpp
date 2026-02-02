#include "footnotestyle.h"

static QString toRoman(int n)
{
    static const struct { int value; const char *numeral; } table[] = {
        {1000, "m"}, {900, "cm"}, {500, "d"}, {400, "cd"},
        {100, "c"}, {90, "xc"}, {50, "l"}, {40, "xl"},
        {10, "x"}, {9, "ix"}, {5, "v"}, {4, "iv"}, {1, "i"}
    };
    QString result;
    for (const auto &entry : table) {
        while (n >= entry.value) {
            result += QString::fromLatin1(entry.numeral);
            n -= entry.value;
        }
    }
    return result;
}

static QString toAlpha(int n)
{
    // 1->a, 2->b, ... 26->z, 27->aa, 28->ab, ...
    QString result;
    while (n > 0) {
        n--; // make 0-based
        result.prepend(QChar(QLatin1Char('a').unicode() + (n % 26)));
        n /= 26;
    }
    return result;
}

static QString toAsterisk(int n)
{
    // 1->*, 2->†, 3->‡, 4->**, 5->††, 6->‡‡, ...
    static const QChar symbols[] = {
        QChar(0x002A), // *
        QChar(0x2020), // †
        QChar(0x2021), // ‡
    };
    int cycle = (n - 1) / 3;
    int idx = (n - 1) % 3;
    return QString(cycle + 1, symbols[idx]);
}

QString FootnoteStyle::formatNumber(int n) const
{
    QString num;
    switch (format) {
    case Arabic:
        num = QString::number(n);
        break;
    case RomanLower:
        num = toRoman(n);
        break;
    case RomanUpper:
        num = toRoman(n).toUpper();
        break;
    case AlphaLower:
        num = toAlpha(n);
        break;
    case AlphaUpper:
        num = toAlpha(n).toUpper();
        break;
    case Asterisk:
        num = toAsterisk(n);
        break;
    }
    return prefix + num + suffix;
}
