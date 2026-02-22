/*
 * fontpairingmanager.h — Discovery/loading/saving for font pairings
 *
 * Scans built-in Qt resources (:/pairings/) and the user data
 * directory for JSON font pairing files and presents them by ID.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef PRETTYREADER_FONTPAIRINGMANAGER_H
#define PRETTYREADER_FONTPAIRINGMANAGER_H

#include <QObject>
#include <QString>
#include <QStringList>

#include "fontpairing.h"

class FontPairingManager : public QObject
{
    Q_OBJECT

public:
    explicit FontPairingManager(QObject *parent = nullptr);

    /// List of all available pairing IDs (built-in + user).
    QStringList availablePairings() const;

    /// Display name for a pairing ID.
    QString pairingName(const QString &id) const;

    /// Load a font pairing by ID.
    FontPairing pairing(const QString &id) const;

    /// Save a user font pairing. Returns the assigned ID.
    QString savePairing(const FontPairing &pairing);

    /// Delete a user font pairing.
    bool deletePairing(const QString &id);

    /// Whether a pairing is built-in (read-only).
    bool isBuiltin(const QString &id) const;

signals:
    void pairingsChanged();

private:
    void discoverPairings();

    struct PairingInfo {
        QString id;
        QString name;
        QString path;
        bool builtin = false;
    };
    QList<PairingInfo> m_pairings;
};

#endif // PRETTYREADER_FONTPAIRINGMANAGER_H
