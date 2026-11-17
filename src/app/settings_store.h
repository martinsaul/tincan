// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include "core/account_config.h"

#include <QList>
#include <QSettings>
#include <QString>
#include <QStringList>

namespace tincan {

struct GeneralSettings {
    bool startMinimized = false;
    bool closeToTray = false;
    bool autostart = false;
    QString ringtonePath;
};

struct AudioSettings {
    QString captureName;
    QString playbackName;
    QString ringName;
    bool echoCancel = true;
    int ecTailMs = 200;
    QStringList codecOrder;
    QStringList disabledCodecs;
};

class SettingsStore {
public:
    explicit SettingsStore(QString filePath = {});

    QString filePath() const;

    QList<AccountConfig> accounts() const;
    void setAccounts(const QList<AccountConfig> &accounts);

    QString defaultAccountId() const;
    void setDefaultAccountId(const QString &accountId);

    GeneralSettings generalSettings() const;
    void setGeneralSettings(const GeneralSettings &settings);

    AudioSettings audioSettings() const;
    void setAudioSettings(const AudioSettings &settings);

private:
    void syncAndRestrictPermissions();

    QString filePath_;
    mutable QSettings settings_;
};

} // namespace tincan
