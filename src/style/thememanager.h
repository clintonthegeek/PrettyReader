#ifndef PRETTYREADER_THEMEMANAGER_H
#define PRETTYREADER_THEMEMANAGER_H

#include <QObject>
#include <QString>
#include <QStringList>

#include "characterstyle.h"
#include "footnotestyle.h"
#include "pagelayout.h"
#include "paragraphstyle.h"
#include "tablestyle.h"

class StyleManager;

class ThemeManager : public QObject
{
    Q_OBJECT

public:
    explicit ThemeManager(QObject *parent = nullptr);

    QStringList availableThemes() const;
    QString themeName(const QString &themeId) const;

    // Load a theme into the given StyleManager
    bool loadTheme(const QString &themeId, StyleManager *styleManager);

    // Create a default StyleManager with built-in defaults
    void loadDefaults(StyleManager *styleManager);

    // Get the page layout from the last loaded theme (if specified)
    PageLayout themePageLayout() const { return m_themePageLayout; }

    // Theme management (M22)
    QString saveTheme(const QString &name, StyleManager *sm, const PageLayout &layout);
    bool saveThemeAs(const QString &themeId, StyleManager *sm, const PageLayout &layout);
    bool deleteTheme(const QString &themeId);
    bool renameTheme(const QString &themeId, const QString &newName);
    bool isBuiltinTheme(const QString &themeId) const;

signals:
    void themesChanged();

private:
    bool loadThemeFromJson(const QString &path, StyleManager *styleManager);
    void registerBuiltinThemes();
    void assignDefaultParents(StyleManager *sm);
    void resolveAllStyles(StyleManager *sm);

    // Serialization helpers
    static QJsonObject serializeParagraphStyle(const ParagraphStyle &style);
    static QJsonObject serializeCharacterStyle(const CharacterStyle &style);
    static QJsonObject serializeTableStyle(const TableStyle &style);
    static QJsonObject serializePageLayout(const PageLayout &layout);
    static QJsonObject serializeMasterPage(const MasterPage &mp);
    static QJsonObject serializeFootnoteStyle(const FootnoteStyle &style);
    QJsonDocument serializeTheme(const QString &name, StyleManager *sm, const PageLayout &layout);

    struct ThemeInfo {
        QString id;
        QString name;
        QString path;
    };
    QList<ThemeInfo> m_themes;
    PageLayout m_themePageLayout;
};

#endif // PRETTYREADER_THEMEMANAGER_H
