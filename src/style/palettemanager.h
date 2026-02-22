/*
 * palettemanager.h — Discovery/loading/saving for color palettes
 *
 * Scans built-in Qt resources (:/palettes/) and the user data
 * directory for JSON palette files and presents them by ID.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef PRETTYREADER_PALETTEMANAGER_H
#define PRETTYREADER_PALETTEMANAGER_H

#include <QObject>
#include <QString>
#include <QStringList>

#include "colorpalette.h"

class PaletteManager : public QObject
{
    Q_OBJECT

public:
    explicit PaletteManager(QObject *parent = nullptr);

    /// List of all available palette IDs (built-in + user).
    QStringList availablePalettes() const;

    /// Display name for a palette ID.
    QString paletteName(const QString &id) const;

    /// Load a palette by ID.
    ColorPalette palette(const QString &id) const;

    /// Save a user palette. Returns the assigned ID.
    QString savePalette(const ColorPalette &palette);

    /// Delete a user palette.
    bool deletePalette(const QString &id);

    /// Whether a palette is built-in (read-only).
    bool isBuiltin(const QString &id) const;

signals:
    void palettesChanged();

private:
    void discoverPalettes();

    struct PaletteInfo {
        QString id;
        QString name;
        QString path;
        bool builtin = false;
    };
    QList<PaletteInfo> m_palettes;
};

#endif // PRETTYREADER_PALETTEMANAGER_H
