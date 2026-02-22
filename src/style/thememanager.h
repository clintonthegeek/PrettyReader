#ifndef PRETTYREADER_THEMEMANAGER_H
#define PRETTYREADER_THEMEMANAGER_H

#include <QObject>
#include <QString>
#include <QStringList>

#include "characterstyle.h"
#include "colorpalette.h"
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

    // Create a default StyleManager with built-in defaults
    void loadDefaults(StyleManager *styleManager);

    // Ensure parent hierarchy is intact after external modifications
    void assignDefaultParents(StyleManager *sm);

    // Get the page layout from the last loaded theme (if specified)
    PageLayout themePageLayout() const { return m_themePageLayout; }

    // Apply style overrides from a JSON object to a StyleManager
    void applyStyleOverrides(const QJsonObject &root, StyleManager *sm);

private:
    void resolveAllStyles(StyleManager *sm);

    PageLayout m_themePageLayout;
};

#endif // PRETTYREADER_THEMEMANAGER_H
