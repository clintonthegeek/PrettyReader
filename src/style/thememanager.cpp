#include "thememanager.h"
#include "stylemanager.h"
#include "paragraphstyle.h"
#include "characterstyle.h"
#include "tablestyle.h"
#include "footnotestyle.h"
#include "fontfeatures.h"
#include "masterpage.h"
#include "colorpalette.h"
#include "fontpairing.h"
#include "palettemanager.h"
#include "fontpairingmanager.h"
#include "themecomposer.h"

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

    // Apply paragraph, character, and table styles + page layout + footnote
    applyStyleOverrides(root, sm);

    // Assign default parents to styles that don't have one
    assignDefaultParents(sm);

    return true;
}

bool ThemeManager::loadPreset(const QString &path,
                              PaletteManager *paletteMgr,
                              FontPairingManager *pairingMgr,
                              StyleManager *sm)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (doc.isNull())
        return false;

    QJsonObject root = doc.object();

    // Verify this is a preset file
    QString type = root.value(QLatin1String("type")).toString();
    if (type != QLatin1String("themePreset"))
        return false;

    // Load palette and pairing by ID
    QString paletteId = root.value(QLatin1String("paletteId")).toString();
    QString pairingId = root.value(QLatin1String("pairingId")).toString();

    ColorPalette palette;
    if (paletteMgr && !paletteId.isEmpty())
        palette = paletteMgr->palette(paletteId);

    FontPairing pairing;
    if (pairingMgr && !pairingId.isEmpty())
        pairing = pairingMgr->pairing(pairingId);

    // Compose palette + pairing into the StyleManager
    ThemeComposer composer(this);
    composer.setColorPalette(palette);
    composer.setFontPairing(pairing);
    composer.compose(sm);

    // Apply style overrides on top (from "styleOverrides" section)
    if (root.contains(QLatin1String("styleOverrides"))) {
        QJsonObject overrides = root.value(QLatin1String("styleOverrides")).toObject();
        applyStyleOverrides(overrides, sm);
    }

    // Apply page layout, master pages, and footnote style from root level
    applyStyleOverrides(root, sm);

    // Apply palette's pageBackground AFTER root-level overrides, because
    // applyStyleOverrides() resets m_themePageLayout when a pageLayout key
    // is present and the preset's pageLayout may not specify pageBackground.
    QColor pageBg = palette.pageBackground();
    if (pageBg.isValid())
        m_themePageLayout.pageBackground = pageBg;

    // Ensure hierarchy after overrides
    assignDefaultParents(sm);

    return true;
}

void ThemeManager::applyStyleOverrides(const QJsonObject &root, StyleManager *sm)
{
    // Paragraph styles
    QJsonObject paraStyles = root.value(QLatin1String("paragraphStyles")).toObject();
    for (auto it = paraStyles.begin(); it != paraStyles.end(); ++it) {
        QJsonObject props = it.value().toObject();

        // If this style already exists, get it and modify; otherwise create new
        ParagraphStyle *existing = sm->paragraphStyle(it.key());
        ParagraphStyle style = existing ? *existing : ParagraphStyle(it.key());

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
        if (props.contains(QLatin1String("fontFeatures"))) {
            QJsonArray features = props.value(QLatin1String("fontFeatures")).toArray();
            QStringList featureList;
            for (const auto &f : features)
                featureList.append(f.toString());
            style.setFontFeatures(FontFeatures::fromStringList(featureList));
        }
        if (props.contains(QLatin1String("baseCharacterStyle")))
            style.setBaseCharacterStyleName(props.value(QLatin1String("baseCharacterStyle")).toString());

        sm->addParagraphStyle(style);
    }

    // Character styles
    QJsonObject charStyles = root.value(QLatin1String("characterStyles")).toObject();
    for (auto it = charStyles.begin(); it != charStyles.end(); ++it) {
        QJsonObject props = it.value().toObject();

        CharacterStyle *existing = sm->characterStyle(it.key());
        CharacterStyle style = existing ? *existing : CharacterStyle(it.key());

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
        if (props.contains(QLatin1String("fontFeatures"))) {
            QJsonArray features = props.value(QLatin1String("fontFeatures")).toArray();
            QStringList featureList;
            for (const auto &f : features)
                featureList.append(f.toString());
            style.setFontFeatures(FontFeatures::fromStringList(featureList));
        }

        sm->addCharacterStyle(style);
    }

    // Table styles
    if (root.contains(QLatin1String("tableStyles"))) {
        QJsonObject tableStyles = root.value(QLatin1String("tableStyles")).toObject();
        for (auto it = tableStyles.begin(); it != tableStyles.end(); ++it) {
            QJsonObject props = it.value().toObject();

            TableStyle *existing = sm->tableStyle(it.key());
            TableStyle ts = existing ? *existing : TableStyle(it.key());

            if (props.contains(QLatin1String("borderCollapse")))
                ts.setBorderCollapse(props.value(QLatin1String("borderCollapse")).toBool());
            if (props.contains(QLatin1String("cellPadding"))) {
                QJsonObject p = props.value(QLatin1String("cellPadding")).toObject();
                ts.setCellPadding(QMarginsF(
                    p.value(QLatin1String("left")).toDouble(4),
                    p.value(QLatin1String("top")).toDouble(3),
                    p.value(QLatin1String("right")).toDouble(4),
                    p.value(QLatin1String("bottom")).toDouble(3)));
            }
            if (props.contains(QLatin1String("headerBackground")))
                ts.setHeaderBackground(QColor(props.value(QLatin1String("headerBackground")).toString()));
            if (props.contains(QLatin1String("headerForeground")))
                ts.setHeaderForeground(QColor(props.value(QLatin1String("headerForeground")).toString()));
            if (props.contains(QLatin1String("bodyBackground")))
                ts.setBodyBackground(QColor(props.value(QLatin1String("bodyBackground")).toString()));
            if (props.contains(QLatin1String("alternateRowColor")))
                ts.setAlternateRowColor(QColor(props.value(QLatin1String("alternateRowColor")).toString()));
            if (props.contains(QLatin1String("alternateFrequency")))
                ts.setAlternateFrequency(props.value(QLatin1String("alternateFrequency")).toInt(1));

            auto parseBorder = [](const QJsonObject &obj) {
                TableStyle::Border b;
                b.width = obj.value(QLatin1String("width")).toDouble(0.5);
                b.color = QColor(obj.value(QLatin1String("color")).toString(QStringLiteral("#333333")));
                return b;
            };
            if (props.contains(QLatin1String("outerBorder")))
                ts.setOuterBorder(parseBorder(props.value(QLatin1String("outerBorder")).toObject()));
            if (props.contains(QLatin1String("innerBorder")))
                ts.setInnerBorder(parseBorder(props.value(QLatin1String("innerBorder")).toObject()));
            if (props.contains(QLatin1String("headerBottomBorder")))
                ts.setHeaderBottomBorder(parseBorder(props.value(QLatin1String("headerBottomBorder")).toObject()));
            if (props.contains(QLatin1String("headerParagraphStyle")))
                ts.setHeaderParagraphStyle(props.value(QLatin1String("headerParagraphStyle")).toString());
            if (props.contains(QLatin1String("bodyParagraphStyle")))
                ts.setBodyParagraphStyle(props.value(QLatin1String("bodyParagraphStyle")).toString());

            sm->addTableStyle(ts);
        }
    }

    // Optional page layout
    if (root.contains(QLatin1String("pageLayout"))) {
        m_themePageLayout = PageLayout{};
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
        if (plObj.contains(QLatin1String("pageBackground"))) {
            m_themePageLayout.pageBackground = QColor(
                plObj.value(QLatin1String("pageBackground")).toString());
        }
    }

    // Master pages
    if (root.contains(QLatin1String("masterPages"))) {
        QJsonObject mpObj = root.value(QLatin1String("masterPages")).toObject();
        for (auto it = mpObj.begin(); it != mpObj.end(); ++it) {
            QJsonObject props = it.value().toObject();
            MasterPage mp;
            mp.name = it.key();

            if (props.contains(QLatin1String("headerEnabled")))
                mp.headerEnabled = props.value(QLatin1String("headerEnabled")).toBool() ? 1 : 0;
            if (props.contains(QLatin1String("footerEnabled")))
                mp.footerEnabled = props.value(QLatin1String("footerEnabled")).toBool() ? 1 : 0;

            if (props.contains(QLatin1String("headerLeft"))) {
                mp.headerLeft = props.value(QLatin1String("headerLeft")).toString();
                mp.hasHeaderLeft = true;
            }
            if (props.contains(QLatin1String("headerCenter"))) {
                mp.headerCenter = props.value(QLatin1String("headerCenter")).toString();
                mp.hasHeaderCenter = true;
            }
            if (props.contains(QLatin1String("headerRight"))) {
                mp.headerRight = props.value(QLatin1String("headerRight")).toString();
                mp.hasHeaderRight = true;
            }
            if (props.contains(QLatin1String("footerLeft"))) {
                mp.footerLeft = props.value(QLatin1String("footerLeft")).toString();
                mp.hasFooterLeft = true;
            }
            if (props.contains(QLatin1String("footerCenter"))) {
                mp.footerCenter = props.value(QLatin1String("footerCenter")).toString();
                mp.hasFooterCenter = true;
            }
            if (props.contains(QLatin1String("footerRight"))) {
                mp.footerRight = props.value(QLatin1String("footerRight")).toString();
                mp.hasFooterRight = true;
            }

            if (props.contains(QLatin1String("margins"))) {
                QJsonObject m = props.value(QLatin1String("margins")).toObject();
                if (m.contains(QLatin1String("top")))    mp.marginTop    = m.value(QLatin1String("top")).toDouble();
                if (m.contains(QLatin1String("bottom"))) mp.marginBottom = m.value(QLatin1String("bottom")).toDouble();
                if (m.contains(QLatin1String("left")))   mp.marginLeft   = m.value(QLatin1String("left")).toDouble();
                if (m.contains(QLatin1String("right")))  mp.marginRight  = m.value(QLatin1String("right")).toDouble();
            }

            m_themePageLayout.masterPages.insert(mp.name, mp);
        }
    }

    // Footnote style
    if (root.contains(QLatin1String("footnoteStyle"))) {
        QJsonObject fnObj = root.value(QLatin1String("footnoteStyle")).toObject();
        FootnoteStyle fs;

        if (fnObj.contains(QLatin1String("format"))) {
            QString fmt = fnObj.value(QLatin1String("format")).toString();
            if (fmt == QLatin1String("roman_lower"))       fs.format = FootnoteStyle::RomanLower;
            else if (fmt == QLatin1String("roman_upper"))  fs.format = FootnoteStyle::RomanUpper;
            else if (fmt == QLatin1String("alpha_lower"))  fs.format = FootnoteStyle::AlphaLower;
            else if (fmt == QLatin1String("alpha_upper"))  fs.format = FootnoteStyle::AlphaUpper;
            else if (fmt == QLatin1String("asterisk"))     fs.format = FootnoteStyle::Asterisk;
            else                                            fs.format = FootnoteStyle::Arabic;
        }
        if (fnObj.contains(QLatin1String("startNumber")))
            fs.startNumber = fnObj.value(QLatin1String("startNumber")).toInt(1);
        if (fnObj.contains(QLatin1String("restart"))) {
            QString r = fnObj.value(QLatin1String("restart")).toString();
            fs.restart = (r == QLatin1String("per_page"))
                ? FootnoteStyle::PerPage : FootnoteStyle::PerDocument;
        }
        if (fnObj.contains(QLatin1String("prefix")))
            fs.prefix = fnObj.value(QLatin1String("prefix")).toString();
        if (fnObj.contains(QLatin1String("suffix")))
            fs.suffix = fnObj.value(QLatin1String("suffix")).toString();
        if (fnObj.contains(QLatin1String("superscriptRef")))
            fs.superscriptRef = fnObj.value(QLatin1String("superscriptRef")).toBool(true);
        if (fnObj.contains(QLatin1String("superscriptNote")))
            fs.superscriptNote = fnObj.value(QLatin1String("superscriptNote")).toBool(false);
        if (fnObj.contains(QLatin1String("asEndnotes")))
            fs.asEndnotes = fnObj.value(QLatin1String("asEndnotes")).toBool(true);
        if (fnObj.contains(QLatin1String("showSeparator")))
            fs.showSeparator = fnObj.value(QLatin1String("showSeparator")).toBool(true);
        if (fnObj.contains(QLatin1String("separatorWidth")))
            fs.separatorWidth = fnObj.value(QLatin1String("separatorWidth")).toDouble(0.5);
        if (fnObj.contains(QLatin1String("separatorLength")))
            fs.separatorLength = fnObj.value(QLatin1String("separatorLength")).toDouble(72.0);

        sm->setFootnoteStyle(fs);
    }
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
        {"BodyText",           "Default Paragraph Style"},
        {"BlockQuote",         "BodyText"},
        {"ListItem",           "BodyText"},
        {"OrderedListItem",    "ListItem"},
        {"UnorderedListItem",  "ListItem"},
        {"TaskListItem",       "ListItem"},
        {"TableCell",          "BodyText"},
        {"Heading1",           "Heading"},
        {"Heading2",           "Heading"},
        {"Heading3",           "Heading"},
        {"Heading4",           "Heading"},
        {"Heading5",           "Heading"},
        {"Heading6",           "Heading"},
        {"CodeBlock",          "Default Paragraph Style"},
        {"TableHeader",        "Default Paragraph Style"},
        {"TableBody",          "Default Paragraph Style"},
        {"Heading",            "Default Paragraph Style"},
        {"HorizontalRule",     "Default Paragraph Style"},
        {"MathDisplay",        "Default Paragraph Style"},
    };

    for (const auto &def : paraDefaults) {
        QString styleName = QString::fromLatin1(def.styleName);
        ParagraphStyle *s = sm->paragraphStyle(styleName);
        if (!s) {
            // Create stub paragraph style
            ParagraphStyle stub(styleName);
            stub.setParentStyleName(QString::fromLatin1(def.parentName));
            sm->addParagraphStyle(stub);
        } else if (s->parentStyleName().isEmpty()) {
            s->setParentStyleName(QString::fromLatin1(def.parentName));
        }
    }

    // Set CodeBlock's baseCharacterStyle if not already set
    ParagraphStyle *codeBlock = sm->paragraphStyle(QStringLiteral("CodeBlock"));
    if (codeBlock && !codeBlock->hasBaseCharacterStyle())
        codeBlock->setBaseCharacterStyleName(QStringLiteral("Code"));

    // Default character hierarchy:
    //   Default Character Style
    //   ├── Emphasis, Strong, StrongEmphasis, Strikethrough, Subscript, Superscript
    //   ├── Code
    //   │   └── InlineCode
    //   ├── Link
    //   ├── Emoji, MathInline

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

    // Ensure "Code" character style exists (shared monospace base)
    if (!sm->characterStyle(QStringLiteral("Code"))) {
        CharacterStyle code(QStringLiteral("Code"));
        code.setParentStyleName(QStringLiteral("Default Character Style"));
        code.setFontFamily(QStringLiteral("JetBrains Mono"));
        code.setFontSize(10.0);
        sm->addCharacterStyle(code);
    }

    static const DefaultParentMap charDefaults[] = {
        {"DefaultText",     "Default Character Style"},
        {"Emphasis",        "Default Character Style"},
        {"Strong",          "Default Character Style"},
        {"StrongEmphasis",  "Default Character Style"},
        {"InlineCode",      "Code"},
        {"Link",            "Default Character Style"},
        {"Strikethrough",   "Default Character Style"},
        {"Subscript",       "Default Character Style"},
        {"Superscript",     "Default Character Style"},
        {"Emoji",           "Default Character Style"},
        {"MathInline",      "Default Character Style"},
        {"Code",            "Default Character Style"},
    };

    for (const auto &def : charDefaults) {
        QString styleName = QString::fromLatin1(def.styleName);
        CharacterStyle *s = sm->characterStyle(styleName);
        if (!s) {
            // Create stub character style
            CharacterStyle stub(styleName);
            stub.setParentStyleName(QString::fromLatin1(def.parentName));
            sm->addCharacterStyle(stub);
        } else if (s->parentStyleName().isEmpty()) {
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

    ParagraphStyle codeBlk(QStringLiteral("CodeBlock"));
    codeBlk.setParentStyleName(QStringLiteral("Default Paragraph Style"));
    codeBlk.setBaseCharacterStyleName(QStringLiteral("Code"));
    codeBlk.setBackground(QColor(0xf6, 0xf8, 0xfa));
    sm->addParagraphStyle(codeBlk);

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

    CharacterStyle codeChar(QStringLiteral("Code"));
    codeChar.setParentStyleName(QStringLiteral("Default Character Style"));
    codeChar.setFontFamily(QStringLiteral("JetBrains Mono"));
    codeChar.setFontSize(10.0);
    sm->addCharacterStyle(codeChar);

    CharacterStyle inlineCode(QStringLiteral("InlineCode"));
    inlineCode.setParentStyleName(QStringLiteral("Code"));
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
    if (style.hasFontFeatures()) {
        QStringList features = FontFeatures::toStringList(style.fontFeatures());
        QJsonArray arr;
        for (const QString &f : features)
            arr.append(f);
        obj[QLatin1String("fontFeatures")] = arr;
    }
    if (style.hasBaseCharacterStyle())
        obj[QLatin1String("baseCharacterStyle")] = style.baseCharacterStyleName();
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
    if (style.hasFontFeatures()) {
        QStringList features = FontFeatures::toStringList(style.fontFeatures());
        QJsonArray arr;
        for (const QString &f : features)
            arr.append(f);
        obj[QLatin1String("fontFeatures")] = arr;
    }
    return obj;
}

QJsonObject ThemeManager::serializeTableStyle(const TableStyle &style)
{
    QJsonObject obj;
    obj[QLatin1String("borderCollapse")] = style.borderCollapse();

    QJsonObject padding;
    padding[QLatin1String("top")]    = style.cellPadding().top();
    padding[QLatin1String("bottom")] = style.cellPadding().bottom();
    padding[QLatin1String("left")]   = style.cellPadding().left();
    padding[QLatin1String("right")]  = style.cellPadding().right();
    obj[QLatin1String("cellPadding")] = padding;

    if (style.hasHeaderBackground())
        obj[QLatin1String("headerBackground")] = style.headerBackground().name();
    if (style.hasHeaderForeground())
        obj[QLatin1String("headerForeground")] = style.headerForeground().name();
    if (style.hasBodyBackground())
        obj[QLatin1String("bodyBackground")] = style.bodyBackground().name();
    if (style.hasAlternateRowColor())
        obj[QLatin1String("alternateRowColor")] = style.alternateRowColor().name();
    if (style.alternateFrequency() != 1)
        obj[QLatin1String("alternateFrequency")] = style.alternateFrequency();

    auto serializeBorder = [](const TableStyle::Border &b) {
        QJsonObject bObj;
        bObj[QLatin1String("width")] = b.width;
        bObj[QLatin1String("color")] = b.color.name();
        return bObj;
    };

    obj[QLatin1String("outerBorder")] = serializeBorder(style.outerBorder());
    obj[QLatin1String("innerBorder")] = serializeBorder(style.innerBorder());
    obj[QLatin1String("headerBottomBorder")] = serializeBorder(style.headerBottomBorder());

    if (!style.headerParagraphStyle().isEmpty())
        obj[QLatin1String("headerParagraphStyle")] = style.headerParagraphStyle();
    if (!style.bodyParagraphStyle().isEmpty())
        obj[QLatin1String("bodyParagraphStyle")] = style.bodyParagraphStyle();

    return obj;
}

QJsonObject ThemeManager::serializeMasterPage(const MasterPage &mp)
{
    QJsonObject obj;

    if (mp.headerEnabled >= 0)
        obj[QLatin1String("headerEnabled")] = (mp.headerEnabled != 0);
    if (mp.footerEnabled >= 0)
        obj[QLatin1String("footerEnabled")] = (mp.footerEnabled != 0);
    if (mp.hasHeaderLeft)
        obj[QLatin1String("headerLeft")] = mp.headerLeft;
    if (mp.hasHeaderCenter)
        obj[QLatin1String("headerCenter")] = mp.headerCenter;
    if (mp.hasHeaderRight)
        obj[QLatin1String("headerRight")] = mp.headerRight;
    if (mp.hasFooterLeft)
        obj[QLatin1String("footerLeft")] = mp.footerLeft;
    if (mp.hasFooterCenter)
        obj[QLatin1String("footerCenter")] = mp.footerCenter;
    if (mp.hasFooterRight)
        obj[QLatin1String("footerRight")] = mp.footerRight;

    if (mp.marginTop >= 0 || mp.marginBottom >= 0
        || mp.marginLeft >= 0 || mp.marginRight >= 0) {
        QJsonObject m;
        if (mp.marginTop >= 0)    m[QLatin1String("top")]    = mp.marginTop;
        if (mp.marginBottom >= 0) m[QLatin1String("bottom")] = mp.marginBottom;
        if (mp.marginLeft >= 0)   m[QLatin1String("left")]   = mp.marginLeft;
        if (mp.marginRight >= 0)  m[QLatin1String("right")]  = mp.marginRight;
        obj[QLatin1String("margins")] = m;
    }

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

    if (layout.pageBackground != Qt::white)
        obj[QLatin1String("pageBackground")] = layout.pageBackground.name();

    return obj;
}

QJsonObject ThemeManager::serializeFootnoteStyle(const FootnoteStyle &style)
{
    QJsonObject obj;

    auto formatStr = [](FootnoteStyle::NumberFormat f) -> QString {
        switch (f) {
        case FootnoteStyle::RomanLower: return QStringLiteral("roman_lower");
        case FootnoteStyle::RomanUpper: return QStringLiteral("roman_upper");
        case FootnoteStyle::AlphaLower: return QStringLiteral("alpha_lower");
        case FootnoteStyle::AlphaUpper: return QStringLiteral("alpha_upper");
        case FootnoteStyle::Asterisk:   return QStringLiteral("asterisk");
        default:                         return QStringLiteral("arabic");
        }
    };

    obj[QLatin1String("format")] = formatStr(style.format);
    obj[QLatin1String("startNumber")] = style.startNumber;
    obj[QLatin1String("restart")] = (style.restart == FootnoteStyle::PerPage)
        ? QStringLiteral("per_page") : QStringLiteral("per_document");
    if (!style.prefix.isEmpty())
        obj[QLatin1String("prefix")] = style.prefix;
    if (!style.suffix.isEmpty())
        obj[QLatin1String("suffix")] = style.suffix;
    obj[QLatin1String("superscriptRef")] = style.superscriptRef;
    obj[QLatin1String("superscriptNote")] = style.superscriptNote;
    obj[QLatin1String("asEndnotes")] = style.asEndnotes;
    obj[QLatin1String("showSeparator")] = style.showSeparator;
    obj[QLatin1String("separatorWidth")] = style.separatorWidth;
    obj[QLatin1String("separatorLength")] = style.separatorLength;

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

    // Table styles
    QStringList tsNames = sm->tableStyleNames();
    if (!tsNames.isEmpty()) {
        QJsonObject tsObj;
        for (const QString &name : tsNames) {
            const TableStyle *ts = sm->tableStyle(name);
            if (ts)
                tsObj[name] = serializeTableStyle(*ts);
        }
        root[QLatin1String("tableStyles")] = tsObj;
    }

    // Page layout
    root[QLatin1String("pageLayout")] = serializePageLayout(layout);

    // Master pages
    if (!layout.masterPages.isEmpty()) {
        QJsonObject mpObj;
        for (auto it = layout.masterPages.begin(); it != layout.masterPages.end(); ++it) {
            if (!it.value().isDefault())
                mpObj[it.key()] = serializeMasterPage(it.value());
        }
        if (!mpObj.isEmpty())
            root[QLatin1String("masterPages")] = mpObj;
    }

    // Footnote style
    root[QLatin1String("footnoteStyle")] = serializeFootnoteStyle(sm->footnoteStyle());

    return QJsonDocument(root);
}

// --- Legacy extraction ---

ColorPalette ThemeManager::extractPalette(const StyleManager *sm, const PageLayout &layout)
{
    ColorPalette palette;
    palette.id   = QStringLiteral("extracted");
    palette.name = QStringLiteral("Extracted from theme");

    const auto &paraStyles = sm->paragraphStyles();
    const auto &charStyles = sm->characterStyles();

    // text ← Default Paragraph Style.foreground
    {
        auto it = paraStyles.find(QStringLiteral("Default Paragraph Style"));
        if (it != paraStyles.end() && it->hasForeground())
            palette.colors[QStringLiteral("text")] = it->foreground();
    }

    // headingText ← Heading.foreground
    {
        auto it = paraStyles.find(QStringLiteral("Heading"));
        if (it != paraStyles.end() && it->hasForeground())
            palette.colors[QStringLiteral("headingText")] = it->foreground();
    }

    // blockquoteText ← BlockQuote.foreground
    {
        auto it = paraStyles.find(QStringLiteral("BlockQuote"));
        if (it != paraStyles.end() && it->hasForeground())
            palette.colors[QStringLiteral("blockquoteText")] = it->foreground();
    }

    // surfaceCode ← CodeBlock.background
    {
        auto it = paraStyles.find(QStringLiteral("CodeBlock"));
        if (it != paraStyles.end() && it->hasBackground())
            palette.colors[QStringLiteral("surfaceCode")] = it->background();
    }

    // linkText ← Link.foreground
    {
        auto it = charStyles.find(QStringLiteral("Link"));
        if (it != charStyles.end() && it->hasForeground())
            palette.colors[QStringLiteral("linkText")] = it->foreground();
    }

    // codeText ← InlineCode.foreground, surfaceInlineCode ← InlineCode.background
    {
        auto it = charStyles.find(QStringLiteral("InlineCode"));
        if (it != charStyles.end()) {
            if (it->hasForeground())
                palette.colors[QStringLiteral("codeText")] = it->foreground();
            if (it->hasBackground())
                palette.colors[QStringLiteral("surfaceInlineCode")] = it->background();
        }
    }

    // Table style colors
    {
        const auto &tableStyles = sm->tableStyles();
        auto it = tableStyles.find(QStringLiteral("Default"));
        if (it != tableStyles.end()) {
            if (it->hasHeaderBackground())
                palette.colors[QStringLiteral("surfaceTableHeader")] = it->headerBackground();
            if (it->hasAlternateRowColor())
                palette.colors[QStringLiteral("surfaceTableAlt")] = it->alternateRowColor();
            if (it->hasOuterBorder())
                palette.colors[QStringLiteral("borderOuter")] = it->outerBorder().color;
            if (it->hasInnerBorder())
                palette.colors[QStringLiteral("borderInner")] = it->innerBorder().color;
            if (it->hasHeaderBottomBorder())
                palette.colors[QStringLiteral("borderHeaderBottom")] = it->headerBottomBorder().color;
        }
    }

    // pageBackground ← PageLayout.pageBackground
    palette.colors[QStringLiteral("pageBackground")] = layout.pageBackground;

    return palette;
}

FontPairing ThemeManager::extractFontPairing(const StyleManager *sm)
{
    FontPairing pairing;
    pairing.id   = QStringLiteral("extracted");
    pairing.name = QStringLiteral("Extracted from theme");

    const auto &paraStyles = sm->paragraphStyles();

    // body ← Default Paragraph Style.fontFamily
    {
        auto it = paraStyles.find(QStringLiteral("Default Paragraph Style"));
        if (it != paraStyles.end() && it->hasFontFamily())
            pairing.body.family = it->fontFamily();
    }

    // heading ← Heading.fontFamily
    {
        auto it = paraStyles.find(QStringLiteral("Heading"));
        if (it != paraStyles.end() && it->hasFontFamily())
            pairing.heading.family = it->fontFamily();
    }

    // mono ← Code.fontFamily (character style)
    {
        const auto &charStyles = sm->characterStyles();
        auto it = charStyles.find(QStringLiteral("Code"));
        if (it != charStyles.end() && it->hasFontFamily())
            pairing.mono.family = it->fontFamily();
    }

    return pairing;
}
