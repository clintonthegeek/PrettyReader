#include "thememanager.h"
#include "stylemanager.h"
#include "paragraphstyle.h"
#include "characterstyle.h"

#include <QColor>
#include <QDir>
#include <QFile>
#include <QFont>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMarginsF>
#include <QPageSize>
#include <QStandardPaths>

ThemeManager::ThemeManager(QObject *parent)
    : QObject(parent)
{
    registerBuiltinThemes();
}

void ThemeManager::registerBuiltinThemes()
{
    // Built-in themes bundled as Qt resources
    QDir resourceDir(QStringLiteral(":/themes"));
    const QStringList entries = resourceDir.entryList(
        {QStringLiteral("*.json")}, QDir::Files);

    for (const QString &entry : entries) {
        QString path = resourceDir.filePath(entry);
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly))
            continue;

        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        if (doc.isNull())
            continue;

        QJsonObject root = doc.object();
        QString id = QFileInfo(entry).completeBaseName();
        QString name = root.value(QLatin1String("name")).toString(id);

        m_themes.append({id, name, path});
    }

    // User themes from XDG data directory
    QString userDir = QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation) + QLatin1String("/themes");
    QDir dir(userDir);
    if (dir.exists()) {
        const QStringList userEntries = dir.entryList(
            {QStringLiteral("*.json")}, QDir::Files);
        for (const QString &entry : userEntries) {
            QString path = dir.filePath(entry);
            QFile file(path);
            if (!file.open(QIODevice::ReadOnly))
                continue;
            QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
            if (doc.isNull())
                continue;
            QJsonObject root = doc.object();
            QString id = QFileInfo(entry).completeBaseName();
            QString name = root.value(QLatin1String("name")).toString(id);
            m_themes.append({id, name, path});
        }
    }
}

QStringList ThemeManager::availableThemes() const
{
    QStringList ids;
    for (const auto &t : m_themes)
        ids.append(t.id);
    return ids;
}

QString ThemeManager::themeName(const QString &themeId) const
{
    for (const auto &t : m_themes) {
        if (t.id == themeId)
            return t.name;
    }
    return themeId;
}

bool ThemeManager::loadTheme(const QString &themeId, StyleManager *sm)
{
    for (const auto &t : m_themes) {
        if (t.id == themeId)
            return loadThemeFromJson(t.path, sm);
    }
    return false;
}

static QFont::Weight parseWeight(const QString &w)
{
    if (w == QLatin1String("bold"))
        return QFont::Bold;
    bool ok;
    int num = w.toInt(&ok);
    if (ok)
        return static_cast<QFont::Weight>(num);
    return QFont::Normal;
}

static Qt::Alignment parseAlignment(const QString &a)
{
    if (a == QLatin1String("center"))  return Qt::AlignCenter;
    if (a == QLatin1String("right"))   return Qt::AlignRight;
    if (a == QLatin1String("justify")) return Qt::AlignJustify;
    return Qt::AlignLeft;
}

bool ThemeManager::loadThemeFromJson(const QString &path, StyleManager *sm)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (doc.isNull())
        return false;

    QJsonObject root = doc.object();

    // Paragraph styles
    QJsonObject paraStyles = root.value(QLatin1String("paragraphStyles")).toObject();
    for (auto it = paraStyles.begin(); it != paraStyles.end(); ++it) {
        QJsonObject props = it.value().toObject();
        ParagraphStyle style(it.key());

        if (props.contains(QLatin1String("fontFamily")))
            style.setFontFamily(props.value(QLatin1String("fontFamily")).toString());
        if (props.contains(QLatin1String("fontSize")))
            style.setFontSize(props.value(QLatin1String("fontSize")).toDouble());
        if (props.contains(QLatin1String("fontWeight")))
            style.setFontWeight(parseWeight(props.value(QLatin1String("fontWeight")).toString()));
        if (props.contains(QLatin1String("fontItalic")))
            style.setFontItalic(props.value(QLatin1String("fontItalic")).toBool());
        if (props.contains(QLatin1String("foreground")))
            style.setForeground(QColor(props.value(QLatin1String("foreground")).toString()));
        if (props.contains(QLatin1String("background")))
            style.setBackground(QColor(props.value(QLatin1String("background")).toString()));
        if (props.contains(QLatin1String("alignment")))
            style.setAlignment(parseAlignment(props.value(QLatin1String("alignment")).toString()));
        if (props.contains(QLatin1String("spaceBefore")))
            style.setSpaceBefore(props.value(QLatin1String("spaceBefore")).toDouble());
        if (props.contains(QLatin1String("spaceAfter")))
            style.setSpaceAfter(props.value(QLatin1String("spaceAfter")).toDouble());
        if (props.contains(QLatin1String("lineHeightPercent")))
            style.setLineHeightPercent(props.value(QLatin1String("lineHeightPercent")).toInt());
        if (props.contains(QLatin1String("firstLineIndent")))
            style.setFirstLineIndent(props.value(QLatin1String("firstLineIndent")).toDouble());
        if (props.contains(QLatin1String("wordSpacing")))
            style.setWordSpacing(props.value(QLatin1String("wordSpacing")).toDouble());

        sm->addParagraphStyle(style);
    }

    // Character styles
    QJsonObject charStyles = root.value(QLatin1String("characterStyles")).toObject();
    for (auto it = charStyles.begin(); it != charStyles.end(); ++it) {
        QJsonObject props = it.value().toObject();
        CharacterStyle style(it.key());

        if (props.contains(QLatin1String("fontFamily")))
            style.setFontFamily(props.value(QLatin1String("fontFamily")).toString());
        if (props.contains(QLatin1String("fontSize")))
            style.setFontSize(props.value(QLatin1String("fontSize")).toDouble());
        if (props.contains(QLatin1String("fontWeight")))
            style.setFontWeight(parseWeight(props.value(QLatin1String("fontWeight")).toString()));
        if (props.contains(QLatin1String("fontItalic")))
            style.setFontItalic(props.value(QLatin1String("fontItalic")).toBool());
        if (props.contains(QLatin1String("underline")))
            style.setFontUnderline(props.value(QLatin1String("underline")).toBool());
        if (props.contains(QLatin1String("strikeOut")))
            style.setFontStrikeOut(props.value(QLatin1String("strikeOut")).toBool());
        if (props.contains(QLatin1String("foreground")))
            style.setForeground(QColor(props.value(QLatin1String("foreground")).toString()));
        if (props.contains(QLatin1String("background")))
            style.setBackground(QColor(props.value(QLatin1String("background")).toString()));
        if (props.contains(QLatin1String("letterSpacing")))
            style.setLetterSpacing(props.value(QLatin1String("letterSpacing")).toDouble());

        sm->addCharacterStyle(style);
    }

    // Inherit: paragraph styles from BodyText, character styles from DefaultText
    ParagraphStyle *bodyText = sm->paragraphStyle(QStringLiteral("BodyText"));
    if (bodyText) {
        for (const QString &name : sm->paragraphStyleNames()) {
            if (name != QLatin1String("BodyText")) {
                ParagraphStyle *s = sm->paragraphStyle(name);
                if (!s)
                    continue;
                bool hadExplicitAlignment = s->hasExplicitAlignment();
                s->inheritFrom(*bodyText);
                // Headings should not inherit justify from BodyText
                if (s->headingLevel() > 0 && !hadExplicitAlignment)
                    s->setAlignment(Qt::AlignLeft);
            }
        }
    }

    CharacterStyle *defaultText = sm->characterStyle(QStringLiteral("DefaultText"));
    if (defaultText) {
        for (const QString &name : sm->characterStyleNames()) {
            if (name != QLatin1String("DefaultText")) {
                CharacterStyle *s = sm->characterStyle(name);
                if (s)
                    s->inheritFrom(*defaultText);
            }
        }
    }

    // Optional page layout
    m_themePageLayout = PageLayout{}; // reset to defaults
    if (root.contains(QLatin1String("pageLayout"))) {
        QJsonObject plObj = root.value(QLatin1String("pageLayout")).toObject();
        if (plObj.contains(QLatin1String("pageSize"))) {
            QString sizeStr = plObj.value(QLatin1String("pageSize")).toString();
            if (sizeStr == QLatin1String("Letter"))      m_themePageLayout.pageSizeId = QPageSize::Letter;
            else if (sizeStr == QLatin1String("A5"))      m_themePageLayout.pageSizeId = QPageSize::A5;
            else if (sizeStr == QLatin1String("Legal"))   m_themePageLayout.pageSizeId = QPageSize::Legal;
            else if (sizeStr == QLatin1String("B5"))      m_themePageLayout.pageSizeId = QPageSize::B5;
            else                                           m_themePageLayout.pageSizeId = QPageSize::A4;
        }
        if (plObj.contains(QLatin1String("orientation"))) {
            QString orient = plObj.value(QLatin1String("orientation")).toString();
            m_themePageLayout.orientation = (orient == QLatin1String("landscape"))
                ? QPageLayout::Landscape : QPageLayout::Portrait;
        }
        if (plObj.contains(QLatin1String("margins"))) {
            QJsonObject m = plObj.value(QLatin1String("margins")).toObject();
            m_themePageLayout.margins = QMarginsF(
                m.value(QLatin1String("left")).toDouble(25.0),
                m.value(QLatin1String("top")).toDouble(25.0),
                m.value(QLatin1String("right")).toDouble(25.0),
                m.value(QLatin1String("bottom")).toDouble(25.0));
        }
    }

    return true;
}

void ThemeManager::loadDefaults(StyleManager *sm)
{
    // Hardcoded defaults if no theme file is available
    ParagraphStyle body(QStringLiteral("BodyText"));
    body.setFontFamily(QStringLiteral("Noto Serif"));
    body.setFontSize(11.0);
    body.setSpaceAfter(6.0);
    body.setLineHeightPercent(150);
    sm->addParagraphStyle(body);

    auto makeHeading = [&](const QString &name, int level, qreal size,
                           qreal before, qreal after) {
        ParagraphStyle h(name);
        h.setFontFamily(QStringLiteral("Noto Sans"));
        h.setFontSize(size);
        h.setFontWeight(QFont::Bold);
        h.setSpaceBefore(before);
        h.setSpaceAfter(after);
        h.setHeadingLevel(level);
        sm->addParagraphStyle(h);
    };

    makeHeading(QStringLiteral("Heading1"), 1, 28, 24, 12);
    makeHeading(QStringLiteral("Heading2"), 2, 24, 20, 10);
    makeHeading(QStringLiteral("Heading3"), 3, 20, 16,  8);
    makeHeading(QStringLiteral("Heading4"), 4, 16, 12,  6);
    makeHeading(QStringLiteral("Heading5"), 5, 14, 10,  4);
    makeHeading(QStringLiteral("Heading6"), 6, 12,  8,  4);

    ParagraphStyle code(QStringLiteral("CodeBlock"));
    code.setFontFamily(QStringLiteral("JetBrains Mono"));
    code.setFontSize(10.0);
    code.setBackground(QColor(0xf6, 0xf8, 0xfa));
    sm->addParagraphStyle(code);

    ParagraphStyle bq(QStringLiteral("BlockQuote"));
    bq.setFontItalic(true);
    bq.setForeground(QColor(0x55, 0x55, 0x55));
    bq.setSpaceAfter(6.0);
    sm->addParagraphStyle(bq);

    CharacterStyle def(QStringLiteral("DefaultText"));
    def.setFontFamily(QStringLiteral("Noto Serif"));
    def.setFontSize(11.0);
    def.setForeground(QColor(0x1a, 0x1a, 0x1a));
    sm->addCharacterStyle(def);

    CharacterStyle inlineCode(QStringLiteral("InlineCode"));
    inlineCode.setFontFamily(QStringLiteral("JetBrains Mono"));
    inlineCode.setFontSize(10.0);
    inlineCode.setForeground(QColor(0xc7, 0x25, 0x4e));
    inlineCode.setBackground(QColor(0xf0, 0xf0, 0xf0));
    sm->addCharacterStyle(inlineCode);

    CharacterStyle link(QStringLiteral("Link"));
    link.setForeground(QColor(0x03, 0x66, 0xd6));
    link.setFontUnderline(true);
    sm->addCharacterStyle(link);
}
