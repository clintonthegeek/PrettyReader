#ifndef PRETTYREADER_THEMEMANAGER_H
#define PRETTYREADER_THEMEMANAGER_H

#include <QObject>
#include <QString>
#include <QStringList>

#include "pagelayout.h"

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

private:
    bool loadThemeFromJson(const QString &path, StyleManager *styleManager);
    void registerBuiltinThemes();

    struct ThemeInfo {
        QString id;
        QString name;
        QString path;
    };
    QList<ThemeInfo> m_themes;
    PageLayout m_themePageLayout;
};

#endif // PRETTYREADER_THEMEMANAGER_H
