/*
 * themecomposer.h — Compose ColorPalette + FontPairing into a StyleManager
 *
 * Applies semantic color roles and typographic roles to the style
 * hierarchy, keeping palette/pairing files portable.  The role-to-style
 * mapping is encapsulated here.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef PRETTYREADER_THEMECOMPOSER_H
#define PRETTYREADER_THEMECOMPOSER_H

#include <QObject>

#include "colorpalette.h"
#include "fontpairing.h"

class StyleManager;
class ThemeManager;

class ThemeComposer : public QObject
{
    Q_OBJECT

public:
    explicit ThemeComposer(ThemeManager *themeManager, QObject *parent = nullptr);

    void setColorPalette(const ColorPalette &palette);
    void setFontPairing(const FontPairing &pairing);

    /// Compose the current palette + pairing into @p target.
    ///
    /// Composition order:
    ///   1. loadDefaults() — hardcoded style hierarchy
    ///   2. Font pairing — set fontFamily on body/heading/mono styles
    ///   3. Color palette — set foreground/background per the role mapping
    ///   4. assignDefaultParents() — ensure parent hierarchy is intact
    void compose(StyleManager *target);

    const ColorPalette &currentPalette() const { return m_palette; }
    const FontPairing &currentPairing() const { return m_pairing; }

signals:
    void compositionChanged();

private:
    void applyFontPairing(StyleManager *target);
    void applyColorPalette(StyleManager *target);

    ThemeManager *m_themeManager = nullptr;
    ColorPalette m_palette;
    FontPairing m_pairing;
};

#endif // PRETTYREADER_THEMECOMPOSER_H
