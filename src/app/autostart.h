// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include <QByteArray>
#include <QString>

namespace tincan {

// Maintains the XDG autostart entry that launches Tincan minimised at login.
class Autostart {
public:
    // An empty directory means "$XDG_CONFIG_HOME/autostart".
    explicit Autostart(QString autostartDir = {});

    QString filePath() const;
    bool isEnabled() const;
    bool apply(bool enabled);

    static QString defaultDirectory();
    static QByteArray desktopEntry();

private:
    QString autostartDir_;
};

} // namespace tincan
