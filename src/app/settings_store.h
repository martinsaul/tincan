// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include "app/auto_answer_policy.h"
#include "core/account_config.h"
#include "core/dtmf_method.h"

#include <QByteArray>
#include <QList>
#include <QSettings>
#include <QString>
#include <QStringList>

namespace tincan {

// How an incoming call is announced: the in-app popup, a desktop
// notification, or both.
enum class IncomingAlertMode { Popup, Notification, Both };

// What to do with passwords when no keychain backend is available.
enum class SecretFallbackChoice { Undecided, ConfigFile, Declined };

struct GeneralSettings {
    bool startMinimized = false;
    bool closeToTray = false;
    bool autostart = false;
    QString ringtonePath;
    IncomingAlertMode incomingAlert = IncomingAlertMode::Both;
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

struct CallSettings {
    DtmfMethod dtmfMethod = DtmfMethod::Rfc4733;
    AutoAnswerMode autoAnswerMode = AutoAnswerMode::Off;
    int autoAnswerDelaySec = 0;
    bool dnd = false;
    QString forwardAlways;
    QString forwardOnBusy;
    QString forwardOnNoAnswer;
    int forwardNoAnswerSec = 15;
    QString recordingsDir;
    bool autoRecord = false;
};

// Main window placement and the tab that was open last.
struct WindowState {
    QByteArray geometry;
    int lastTab = 0;
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

    CallSettings callSettings() const;
    void setCallSettings(const CallSettings &settings);

    SecretFallbackChoice secretFallbackChoice() const;
    void setSecretFallbackChoice(SecretFallbackChoice choice);

    WindowState windowState() const;
    void setWindowState(const WindowState &state);

private:
    void syncAndRestrictPermissions();

    QString filePath_;
    mutable QSettings settings_;
};

} // namespace tincan
