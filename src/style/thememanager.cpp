#include "thememanager.h"
#include "stylemanager.h"
#include "paragraphstyle.h"
#include "characterstyle.h"

#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMarginsF>
#include <QPageSize>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUuid>

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

bool ThemeManager::isBuiltinTheme(const QString &themeId) const
{
    for (const auto &t : m_themes) {
        if (t.id == themeId)
            return t.path.startsWith(QLatin1String(":/"));
    }
    return false;
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

        if (props.contains(QLatin1String("parent")))
            style.setParentStyleName(props.value(QLatin1String("parent")).toString());
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
        if (props.contains(QLatin1String("leftMargin")))
            style.setLeftMargin(props.value(QLatin1String("leftMargin")).toDouble());
        if (props.contains(QLatin1String("rightMargin")))
            style.setRightMargin(props.value(QLatin1String("rightMargin")).toDouble());

        sm->addParagraphStyle(style);
    }

    // Character styles
    QJsonObject charStyles = root.value(QLatin1String("characterStyles")).toObject();
    for (auto it = charStyles.begin(); it != charStyles.end(); ++it) {
        QJsonObject props = it.value().toObject();
        CharacterStyle style(it.key());

        if (props.contains(QLatin1String("parent")))
            style.setParentStyleName(props.value(QLatin1String("parent")).toString());
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

    // Assign default parents to styles that don't have one
    assignDefaultParents(sm);

    // Optional page layout
    m_themePageLayout = PageLayout{};
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
        if (plObj.contains(QLatin1String("header"))) {
            QJsonObject h = plObj.value(QLatin1String("header")).toObject();
            m_themePageLayout.headerEnabled = h.value(QLatin1String("enabled")).toBool(false);
            m_themePageLayout.headerLeft    = h.value(QLatin1String("left")).toString();
            m_themePageLayout.headerCenter  = h.value(QLatin1String("center")).toString();
            m_themePageLayout.headerRight   = h.value(QLatin1String("right")).toString();
        }
        if (plObj.contains(QLatin1String("footer"))) {
            QJsonObject f = plObj.value(QLatin1String("footer")).toObject();
            m_themePageLayout.footerEnabled = f.value(QLatin1String("enabled")).toBool(true);
            m_themePageLayout.footerLeft    = f.value(QLatin1String("left")).toString();
            m_themePageLayout.footerCenter  = f.value(QLatin1String("center")).toString();
            m_themePageLayout.footerRight   = f.value(QLatin1String("right")).toString(
                QStringLiteral("{page} / {pages}"));
        }
    }

    return true;
}

void ThemeManager::assignDefaultParents(StyleManager *sm)
{
    // Default paragraph hierarchy:
    //   Default Paragraph Style
    //   ├── Body Text
    //   │   ├── Block Quotation, List Item, Table Cell
    //   ├── Heading
    //   │   ├── Heading 1-6
    //   ├── Code Block
    //   └── Table Header

    // Ensure abstract parent styles exist
    if (!sm->paragraphStyle(QStringLiteral("Default Paragraph Style"))) {
        ParagraphStyle dps(QStringLiteral("Default Paragraph Style"));
        dps.setFontFamily(QStringLiteral("Noto Serif"));
        dps.setFontSize(11.0);
        dps.setLineHeightPercent(100);
        dps.setForeground(QColor(0x1a, 0x1a, 0x1a));
        sm->addParagraphStyle(dps);
    }

    if (!sm->paragraphStyle(QStringLiteral("Heading"))) {
        ParagraphStyle heading(QStringLiteral("Heading"));
        heading.setParentStyleName(QStringLiteral("Default Paragraph Style"));
        heading.setFontFamily(QStringLiteral("Noto Sans"));
        heading.setFontWeight(QFont::Bold);
        heading.setAlignment(Qt::AlignLeft);
        sm->addParagraphStyle(heading);
    }

    // Assign parents where not explicitly set
    struct DefaultParentMap {
        const char *styleName;
        const char *parentName;
    };

    static const DefaultParentMap paraDefaults[] = {
        {"BodyText",     "Default Paragraph Style"},
        {"BlockQuote",   "BodyText"},
        {"ListItem",     "BodyText"},
        {"TableCell",    "BodyText"},
        {"Heading1",     "Heading"},
        {"Heading2",     "Heading"},
        {"Heading3",     "Heading"},
        {"Heading4",     "Heading"},
        {"Heading5",     "Heading"},
        {"Heading6",     "Heading"},
        {"CodeBlock",    "Default Paragraph Style"},
        {"TableHeader",  "Default Paragraph Style"},
        {"TableBody",    "Default Paragraph Style"},
        {"Heading",      "Default Paragraph Style"},
    };

    for (const auto &def : paraDefaults) {
        ParagraphStyle *s = sm->paragraphStyle(QString::fromLatin1(def.styleName));
        if (s && s->parentStyleName().isEmpty()) {
            s->setParentStyleName(QString::fromLatin1(def.parentName));
        }
    }

    // Default character hierarchy:
    //   Default Character Style
    //   ├── Emphasis, Strong, StrongEmphasis, InlineCode, Link, Strikethrough

    if (!sm->characterStyle(QStringLiteral("Default Character Style"))) {
        CharacterStyle dcs(QStringLiteral("Default Character Style"));
        // Copy from DefaultText if it exists
        CharacterStyle *dt = sm->characterStyle(QStringLiteral("DefaultText"));
        if (dt) {
            if (dt->hasFontFamily()) dcs.setFontFamily(dt->fontFamily());
            if (dt->hasFontSize()) dcs.setFontSize(dt->fontSize());
            if (dt->hasForeground()) dcs.setForeground(dt->foreground());
        } else {
            dcs.setFontFamily(QStringLiteral("Noto Serif"));
            dcs.setFontSize(11.0);
            dcs.setForeground(QColor(0x1a, 0x1a, 0x1a));
        }
        sm->addCharacterStyle(dcs);
    }

    static const DefaultParentMap charDefaults[] = {
        {"DefaultText",     "Default Character Style"},
        {"Emphasis",        "Default Character Style"},
        {"Strong",          "Default Character Style"},
        {"StrongEmphasis",  "Default Character Style"},
        {"InlineCode",      "Default Character Style"},
        {"Link",            "Default Character Style"},
        {"Strikethrough",   "Default Character Style"},
    };

    for (const auto &def : charDefaults) {
        CharacterStyle *s = sm->characterStyle(QString::fromLatin1(def.styleName));
        if (s && s->parentStyleName().isEmpty()) {
            s->setParentStyleName(QString::fromLatin1(def.parentName));
        }
    }
}

void ThemeManager::resolveAllStyles(StyleManager *sm)
{
    // Resolve all paragraph styles through their parent chain
    QStringList paraNames = sm->paragraphStyleNames();
    for (const QString &name : paraNames) {
        ParagraphStyle resolved = sm->resolvedParagraphStyle(name);
        // Preserve the original parent name and heading level
        ParagraphStyle *orig = sm->paragraphStyle(name);
        if (orig) {
            resolved.setParentStyleName(orig->parentStyleName());
            if (orig->headingLevel() > 0)
                resolved.setHeadingLevel(orig->headingLevel());
        }
        sm->addParagraphStyle(resolved);
    }

    // Resolve all character styles through their parent chain
    QStringList charNames = sm->characterStyleNames();
    for (const QString &name : charNames) {
        CharacterStyle resolved = sm->resolvedCharacterStyle(name);
        CharacterStyle *orig = sm->characterStyle(name);
        if (orig) {
            resolved.setParentStyleName(orig->parentStyleName());
        }
        sm->addCharacterStyle(resolved);
    }
}

void ThemeManager::loadDefaults(StyleManager *sm)
{
    // Hardcoded defaults if no theme file is available

    // Abstract parent styles
    ParagraphStyle dps(QStringLiteral("Default Paragraph Style"));
    dps.setFontFamily(QStringLiteral("Noto Serif"));
    dps.setFontSize(11.0);
    dps.setLineHeightPercent(100);
    dps.setForeground(QColor(0x1a, 0x1a, 0x1a));
    sm->addParagraphStyle(dps);

    ParagraphStyle heading(QStringLiteral("Heading"));
    heading.setParentStyleName(QStringLiteral("Default Paragraph Style"));
    heading.setFontFamily(QStringLiteral("Noto Sans"));
    heading.setFontWeight(QFont::Bold);
    heading.setAlignment(Qt::AlignLeft);
    sm->addParagraphStyle(heading);

    ParagraphStyle body(QStringLiteral("BodyText"));
    body.setParentStyleName(QStringLiteral("Default Paragraph Style"));
    body.setSpaceAfter(6.0);
    sm->addParagraphStyle(body);

    auto makeHeading = [&](const QString &name, int level, qreal size,
                           qreal before, qreal after) {
        ParagraphStyle h(name);
        h.setParentStyleName(QStringLiteral("Heading"));
        h.setFontSize(size);
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
    code.setParentStyleName(QStringLiteral("Default Paragraph Style"));
    code.setFontFamily(QStringLiteral("JetBrains Mono"));
    code.setFontSize(10.0);
    code.setBackground(QColor(0xf6, 0xf8, 0xfa));
    sm->addParagraphStyle(code);

    ParagraphStyle bq(QStringLiteral("BlockQuote"));
    bq.setParentStyleName(QStringLiteral("BodyText"));
    bq.setFontItalic(true);
    bq.setForeground(QColor(0x55, 0x55, 0x55));
    sm->addParagraphStyle(bq);

    ParagraphStyle li(QStringLiteral("ListItem"));
    li.setParentStyleName(QStringLiteral("BodyText"));
    sm->addParagraphStyle(li);

    // Character styles
    CharacterStyle dcs(QStringLiteral("Default Character Style"));
    dcs.setFontFamily(QStringLiteral("Noto Serif"));
    dcs.setFontSize(11.0);
    dcs.setForeground(QColor(0x1a, 0x1a, 0x1a));
    sm->addCharacterStyle(dcs);

    CharacterStyle def(QStringLiteral("DefaultText"));
    def.setParentStyleName(QStringLiteral("Default Character Style"));
    sm->addCharacterStyle(def);

    CharacterStyle inlineCode(QStringLiteral("InlineCode"));
    inlineCode.setParentStyleName(QStringLiteral("Default Character Style"));
    inlineCode.setFontFamily(QStringLiteral("JetBrains Mono"));
    inlineCode.setFontSize(10.0);
    inlineCode.setForeground(QColor(0xc7, 0x25, 0x4e));
    inlineCode.setBackground(QColor(0xf0, 0xf0, 0xf0));
    sm->addCharacterStyle(inlineCode);

    CharacterStyle link(QStringLiteral("Link"));
    link.setParentStyleName(QStringLiteral("Default Character Style"));
    link.setForeground(QColor(0x03, 0x66, 0xd6));
    link.setFontUnderline(true);
    sm->addCharacterStyle(link);

}

// --- Theme management (M22) ---

static QString userThemesDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
           + QLatin1String("/themes");
}

QString ThemeManager::saveTheme(const QString &name, StyleManager *sm, const PageLayout &layout)
{
    QString dir = userThemesDir();
    QDir().mkpath(dir);

    // Generate a unique ID
    QString id = name.toLower().replace(QRegularExpression(QStringLiteral("[^a-z0-9]+")),
                                        QStringLiteral("-"));
    if (id.isEmpty())
        id = QStringLiteral("theme");

    // Ensure uniqueness
    QString path = dir + QLatin1Char('/') + id + QLatin1String(".json");
    int suffix = 1;
    while (QFile::exists(path)) {
        id = name.toLower().replace(QRegularExpression(QStringLiteral("[^a-z0-9]+")),
                                    QStringLiteral("-"))
             + QStringLiteral("-") + QString::number(suffix++);
        path = dir + QLatin1Char('/') + id + QLatin1String(".json");
    }

    QJsonDocument doc = serializeTheme(name, sm, layout);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return {};

    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

    m_themes.append({id, name, path});
    Q_EMIT themesChanged();
    return id;
}

bool ThemeManager::saveThemeAs(const QString &themeId, StyleManager *sm, const PageLayout &layout)
{
    // Find existing theme
    for (auto &t : m_themes) {
        if (t.id == themeId) {
            if (t.path.startsWith(QLatin1String(":/")))
                return false; // cannot overwrite built-in

            QJsonDocument doc = serializeTheme(t.name, sm, layout);
            QFile file(t.path);
            if (!file.open(QIODevice::WriteOnly))
                return false;
            file.write(doc.toJson(QJsonDocument::Indented));
            file.close();

            Q_EMIT themesChanged();
            return true;
        }
    }
    return false;
}

bool ThemeManager::deleteTheme(const QString &themeId)
{
    for (int i = 0; i < m_themes.size(); ++i) {
        if (m_themes[i].id == themeId) {
            if (m_themes[i].path.startsWith(QLatin1String(":/")))
                return false; // cannot delete built-in

            QFile::remove(m_themes[i].path);
            m_themes.removeAt(i);
            Q_EMIT themesChanged();
            return true;
        }
    }
    return false;
}

bool ThemeManager::renameTheme(const QString &themeId, const QString &newName)
{
    for (auto &t : m_themes) {
        if (t.id == themeId) {
            if (t.path.startsWith(QLatin1String(":/")))
                return false;

            // Read, modify name, rewrite
            QFile file(t.path);
            if (!file.open(QIODevice::ReadOnly))
                return false;
            QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
            file.close();

            QJsonObject root = doc.object();
            root[QLatin1String("name")] = newName;
            doc.setObject(root);

            if (!file.open(QIODevice::WriteOnly))
                return false;
            file.write(doc.toJson(QJsonDocument::Indented));
            file.close();

            t.name = newName;
            Q_EMIT themesChanged();
            return true;
        }
    }
    return false;
}

// --- Serialization ---

static QString weightToString(QFont::Weight w)
{
    if (w == QFont::Bold)
        return QStringLiteral("bold");
    if (w == QFont::Normal)
        return {};
    return QString::number(static_cast<int>(w));
}

static QString alignmentToString(Qt::Alignment a)
{
    if (a == Qt::AlignCenter || a == Qt::AlignHCenter)
        return QStringLiteral("center");
    if (a == Qt::AlignRight)
        return QStringLiteral("right");
    if (a == Qt::AlignJustify)
        return QStringLiteral("justify");
    return QStringLiteral("left");
}

QJsonObject ThemeManager::serializeParagraphStyle(const ParagraphStyle &style)
{
    QJsonObject obj;
    if (!style.parentStyleName().isEmpty())
        obj[QLatin1String("parent")] = style.parentStyleName();
    if (style.hasFontFamily())
        obj[QLatin1String("fontFamily")] = style.fontFamily();
    if (style.hasFontSize())
        obj[QLatin1String("fontSize")] = style.fontSize();
    if (style.hasFontWeight()) {
        QString ws = weightToString(style.fontWeight());
        if (!ws.isEmpty())
            obj[QLatin1String("fontWeight")] = ws;
    }
    if (style.hasFontItalic())
        obj[QLatin1String("fontItalic")] = style.fontItalic();
    if (style.hasForeground())
        obj[QLatin1String("foreground")] = style.foreground().name();
    if (style.hasBackground())
        obj[QLatin1String("background")] = style.background().name();
    if (style.hasAlignment())
        obj[QLatin1String("alignment")] = alignmentToString(style.alignment());
    if (style.hasSpaceBefore())
        obj[QLatin1String("spaceBefore")] = style.spaceBefore();
    if (style.hasSpaceAfter())
        obj[QLatin1String("spaceAfter")] = style.spaceAfter();
    if (style.hasLineHeight())
        obj[QLatin1String("lineHeightPercent")] = style.lineHeightPercent();
    if (style.hasFirstLineIndent())
        obj[QLatin1String("firstLineIndent")] = style.firstLineIndent();
    if (style.hasWordSpacing())
        obj[QLatin1String("wordSpacing")] = style.wordSpacing();
    if (style.hasLeftMargin())
        obj[QLatin1String("leftMargin")] = style.leftMargin();
    if (style.hasRightMargin())
        obj[QLatin1String("rightMargin")] = style.rightMargin();
    return obj;
}

QJsonObject ThemeManager::serializeCharacterStyle(const CharacterStyle &style)
{
    QJsonObject obj;
    if (!style.parentStyleName().isEmpty())
        obj[QLatin1String("parent")] = style.parentStyleName();
    if (style.hasFontFamily())
        obj[QLatin1String("fontFamily")] = style.fontFamily();
    if (style.hasFontSize())
        obj[QLatin1String("fontSize")] = style.fontSize();
    if (style.hasFontWeight()) {
        QString ws = weightToString(style.fontWeight());
        if (!ws.isEmpty())
            obj[QLatin1String("fontWeight")] = ws;
    }
    if (style.hasFontItalic())
        obj[QLatin1String("fontItalic")] = style.fontItalic();
    if (style.hasFontUnderline())
        obj[QLatin1String("underline")] = style.fontUnderline();
    if (style.hasFontStrikeOut())
        obj[QLatin1String("strikeOut")] = style.fontStrikeOut();
    if (style.hasForeground())
        obj[QLatin1String("foreground")] = style.foreground().name();
    if (style.hasBackground())
        obj[QLatin1String("background")] = style.background().name();
    if (style.hasLetterSpacing())
        obj[QLatin1String("letterSpacing")] = style.letterSpacing();
    return obj;
}

QJsonObject ThemeManager::serializePageLayout(const PageLayout &layout)
{
    QJsonObject obj;

    // Page size
    switch (layout.pageSizeId) {
    case QPageSize::Letter: obj[QLatin1String("pageSize")] = QStringLiteral("Letter"); break;
    case QPageSize::A5:     obj[QLatin1String("pageSize")] = QStringLiteral("A5");     break;
    case QPageSize::Legal:  obj[QLatin1String("pageSize")] = QStringLiteral("Legal");  break;
    case QPageSize::B5:     obj[QLatin1String("pageSize")] = QStringLiteral("B5");     break;
    default:                obj[QLatin1String("pageSize")] = QStringLiteral("A4");     break;
    }

    obj[QLatin1String("orientation")] = (layout.orientation == QPageLayout::Landscape)
        ? QStringLiteral("landscape") : QStringLiteral("portrait");

    QJsonObject margins;
    margins[QLatin1String("left")]   = layout.margins.left();
    margins[QLatin1String("top")]    = layout.margins.top();
    margins[QLatin1String("right")]  = layout.margins.right();
    margins[QLatin1String("bottom")] = layout.margins.bottom();
    obj[QLatin1String("margins")] = margins;

    // Header config
    QJsonObject header;
    header[QLatin1String("enabled")] = layout.headerEnabled;
    header[QLatin1String("left")]    = layout.headerLeft;
    header[QLatin1String("center")]  = layout.headerCenter;
    header[QLatin1String("right")]   = layout.headerRight;
    obj[QLatin1String("header")] = header;

    // Footer config
    QJsonObject footer;
    footer[QLatin1String("enabled")] = layout.footerEnabled;
    footer[QLatin1String("left")]    = layout.footerLeft;
    footer[QLatin1String("center")]  = layout.footerCenter;
    footer[QLatin1String("right")]   = layout.footerRight;
    obj[QLatin1String("footer")] = footer;

    return obj;
}

QJsonDocument ThemeManager::serializeTheme(const QString &name, StyleManager *sm,
                                            const PageLayout &layout)
{
    QJsonObject root;
    root[QLatin1String("name")] = name;
    root[QLatin1String("version")] = 1;

    // Paragraph styles
    QJsonObject paraObj;
    const auto &paraStyles = sm->paragraphStyles();
    for (auto it = paraStyles.begin(); it != paraStyles.end(); ++it) {
        paraObj[it.key()] = serializeParagraphStyle(it.value());
    }
    root[QLatin1String("paragraphStyles")] = paraObj;

    // Character styles
    QJsonObject charObj;
    const auto &charStyles = sm->characterStyles();
    for (auto it = charStyles.begin(); it != charStyles.end(); ++it) {
        charObj[it.key()] = serializeCharacterStyle(it.value());
    }
    root[QLatin1String("characterStyles")] = charObj;

    // Page layout
    root[QLatin1String("pageLayout")] = serializePageLayout(layout);

    return QJsonDocument(root);
}
