/*
 * themecomposer.h — Compose ColorPalette + TypographyTheme into a StyleManager
 *
 * Applies semantic color roles and typographic roles to the style
 * hierarchy, keeping palette/theme files portable.  The role-to-style
 * mapping is encapsulated here.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef PRETTYREADER_THEMECOMPOSER_H
#define PRETTYREADER_THEMECOMPOSER_H

#include <QObject>

#include "colorpalette.h"
#include "typographytheme.h"

class StyleManager;
class ThemeManager;

class ThemeComposer : public QObject
{
    Q_OBJECT

public:
    explicit ThemeComposer(ThemeManager *themeManager, QObject *parent = nullptr);

    void setColorPalette(const ColorPalette &palette);
    void setTypographyTheme(const TypographyTheme &theme);

    /// Compose the current typography theme + palette into @p target.
    ///
    /// Composition order:
    ///   1. loadDefaults() — hardcoded style hierarchy
    ///   2. Typography theme (fonts + style overrides)
    ///   3. Color palette — set foreground/background per the role mapping
    ///   4. assignDefaultParents() — ensure parent hierarchy is intact
    void compose(StyleManager *target);

    const ColorPalette &currentPalette() const { return m_palette; }
    const TypographyTheme &currentTypographyTheme() const { return m_typographyTheme; }

    /// Look up the Hershey fallback for a TTF family.
    /// Dispatches to whichever typography source is active.
    QString hersheyFamilyFor(const QString &ttfFamily) const;

Q_SIGNALS:
    void compositionChanged();

private:
    void applyTypographyTheme(StyleManager *target);
    void applyColorPalette(StyleManager *target);

    ThemeManager *m_themeManager = nullptr;
    ColorPalette m_palette;
    TypographyTheme m_typographyTheme;
};

#endif // PRETTYREADER_THEMECOMPOSER_H
