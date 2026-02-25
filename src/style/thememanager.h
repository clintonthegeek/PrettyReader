#ifndef PRETTYREADER_THEMEMANAGER_H
#define PRETTYREADER_THEMEMANAGER_H

#include <QObject>
#include <QString>
#include <QStringList>

#include "characterstyle.h"
#include "colorpalette.h"
#include "footnotestyle.h"
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

    // Apply style overrides from a JSON object to a StyleManager
    void applyStyleOverrides(const QJsonObject &root, StyleManager *sm);

private:
    void resolveAllStyles(StyleManager *sm);
};

#endif // PRETTYREADER_THEMEMANAGER_H
