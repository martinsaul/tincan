// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "app/settings_store.h"

#include "app/app_paths.h"

#include <QDir>
#include <QFile>
#include <QFileDevice>
#include <QFileInfo>

namespace tincan {

namespace {

QString resolveSettingsPath(const QString &path) {
    if (!path.trimmed().isEmpty()) {
        return path;
    }
    return QDir(AppPaths::configDir()).filePath(QStringLiteral("tincan.ini"));
}

QString alertModeToString(IncomingAlertMode mode) {
    switch (mode) {
    case IncomingAlertMode::Popup:
        return QStringLiteral("popup");
    case IncomingAlertMode::Notification:
        return QStringLiteral("notification");
    case IncomingAlertMode::Both:
        break;
    }
    return QStringLiteral("both");
}

IncomingAlertMode alertModeFromString(const QString &text) {
    if (text == QLatin1String("popup")) {
        return IncomingAlertMode::Popup;
    }
    if (text == QLatin1String("notification")) {
        return IncomingAlertMode::Notification;
    }
    return IncomingAlertMode::Both;
}

void ensurePrivateFileExists(const QString &filePath) {
    const QFileInfo info(filePath);
    QDir().mkpath(info.absolutePath());
    QFile file(filePath);
    if (!file.exists() && file.open(QIODevice::WriteOnly)) {
        file.close();
    }
    QFile::setPermissions(filePath, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
}

QString transportToString(Transport transport) {
    switch (transport) {
    case Transport::Udp:
        return QStringLiteral("udp");
    case Transport::Tcp:
        return QStringLiteral("tcp");
    case Transport::Tls:
        return QStringLiteral("tls");
    }
    return QStringLiteral("udp");
}

Transport transportFromString(const QString &value) {
    const QString lower = value.trimmed().toLower();
    if (lower == QStringLiteral("tcp")) {
        return Transport::Tcp;
    }
    if (lower == QStringLiteral("tls")) {
        return Transport::Tls;
    }
    return Transport::Udp;
}

QString srtpToString(SrtpMode mode) {
    switch (mode) {
    case SrtpMode::Disabled:
        return QStringLiteral("disabled");
    case SrtpMode::Optional:
        return QStringLiteral("optional");
    case SrtpMode::Mandatory:
        return QStringLiteral("mandatory");
    }
    return QStringLiteral("disabled");
}

SrtpMode srtpFromString(const QString &value) {
    const QString lower = value.trimmed().toLower();
    if (lower == QStringLiteral("optional")) {
        return SrtpMode::Optional;
    }
    if (lower == QStringLiteral("mandatory")) {
        return SrtpMode::Mandatory;
    }
    return SrtpMode::Disabled;
}

QString dtmfToString(DtmfMethod method) {
    switch (method) {
    case DtmfMethod::Rfc4733:
        return QStringLiteral("rfc4733");
    case DtmfMethod::SipInfo:
        return QStringLiteral("sipinfo");
    case DtmfMethod::InBand:
        return QStringLiteral("inband");
    }
    return QStringLiteral("rfc4733");
}

DtmfMethod dtmfFromString(const QString &value) {
    const QString lower = value.trimmed().toLower();
    if (lower == QStringLiteral("sipinfo")) {
        return DtmfMethod::SipInfo;
    }
    if (lower == QStringLiteral("inband")) {
        return DtmfMethod::InBand;
    }
    return DtmfMethod::Rfc4733;
}

QString autoAnswerToString(AutoAnswerMode mode) {
    switch (mode) {
    case AutoAnswerMode::Off:
        return QStringLiteral("off");
    case AutoAnswerMode::Always:
        return QStringLiteral("always");
    case AutoAnswerMode::HeaderOnly:
        return QStringLiteral("header");
    }
    return QStringLiteral("off");
}

AutoAnswerMode autoAnswerFromString(const QString &value) {
    const QString lower = value.trimmed().toLower();
    if (lower == QStringLiteral("always")) {
        return AutoAnswerMode::Always;
    }
    if (lower == QStringLiteral("header")) {
        return AutoAnswerMode::HeaderOnly;
    }
    return AutoAnswerMode::Off;
}

AccountConfig readOneAccount(QSettings &s) {
    AccountConfig cfg;
    cfg.id = s.value(QStringLiteral("id")).toString();
    cfg.displayName = s.value(QStringLiteral("displayName")).toString();
    cfg.user = s.value(QStringLiteral("user")).toString();
    cfg.domain = s.value(QStringLiteral("domain")).toString();
    cfg.authUser = s.value(QStringLiteral("authUser")).toString();
    cfg.proxy = s.value(QStringLiteral("proxy")).toString();
    cfg.transport = transportFromString(s.value(QStringLiteral("transport")).toString());
    cfg.srtp = srtpFromString(s.value(QStringLiteral("srtp")).toString());
    cfg.registerAccount = s.value(QStringLiteral("registerAccount"), true).toBool();
    cfg.regExpirySec = s.value(QStringLiteral("regExpirySec"), 300).toInt();
    cfg.stunServer = s.value(QStringLiteral("stunServer")).toString();
    cfg.ice = s.value(QStringLiteral("ice"), false).toBool();
    cfg.publicAddress = s.value(QStringLiteral("publicAddress")).toString();
    cfg.dialPrefix = s.value(QStringLiteral("dialPrefix")).toString();
    cfg.voicemailNumber = s.value(QStringLiteral("voicemailNumber")).toString();
    cfg.publishPresence = s.value(QStringLiteral("publishPresence"), false).toBool();
    return cfg;
}

void writeOneAccount(QSettings &s, const AccountConfig &cfg) {
    // Passwords are NEVER written to SettingsStore.
    s.setValue(QStringLiteral("id"), cfg.id);
    s.setValue(QStringLiteral("displayName"), cfg.displayName);
    s.setValue(QStringLiteral("user"), cfg.user);
    s.setValue(QStringLiteral("domain"), cfg.domain);
    s.setValue(QStringLiteral("authUser"), cfg.authUser);
    s.setValue(QStringLiteral("proxy"), cfg.proxy);
    s.setValue(QStringLiteral("transport"), transportToString(cfg.transport));
    s.setValue(QStringLiteral("srtp"), srtpToString(cfg.srtp));
    s.setValue(QStringLiteral("registerAccount"), cfg.registerAccount);
    s.setValue(QStringLiteral("regExpirySec"), cfg.regExpirySec);
    s.setValue(QStringLiteral("stunServer"), cfg.stunServer);
    s.setValue(QStringLiteral("ice"), cfg.ice);
    s.setValue(QStringLiteral("publicAddress"), cfg.publicAddress);
    s.setValue(QStringLiteral("dialPrefix"), cfg.dialPrefix);
    s.setValue(QStringLiteral("voicemailNumber"), cfg.voicemailNumber);
    s.setValue(QStringLiteral("publishPresence"), cfg.publishPresence);
}

} // namespace

SettingsStore::SettingsStore(QString filePath)
    : filePath_(resolveSettingsPath(filePath)), settings_(filePath_, QSettings::IniFormat) {
    ensurePrivateFileExists(filePath_);
}

QString SettingsStore::filePath() const {
    return filePath_;
}

QList<AccountConfig> SettingsStore::accounts() const {
    const int count = settings_.beginReadArray(QStringLiteral("accounts"));
    QList<AccountConfig> result;
    result.reserve(count);
    for (int i = 0; i < count; ++i) {
        settings_.setArrayIndex(i);
        result.append(readOneAccount(settings_));
    }
    settings_.endArray();
    return result;
}

void SettingsStore::setAccounts(const QList<AccountConfig> &accounts) {
    settings_.remove(QStringLiteral("accounts"));
    settings_.beginWriteArray(QStringLiteral("accounts"), static_cast<int>(accounts.size()));
    for (qsizetype i = 0; i < accounts.size(); ++i) {
        settings_.setArrayIndex(static_cast<int>(i));
        writeOneAccount(settings_, accounts[i]);
    }
    settings_.endArray();
    syncAndRestrictPermissions();
}

QString SettingsStore::defaultAccountId() const {
    return settings_.value(QStringLiteral("general/defaultAccountId")).toString();
}

void SettingsStore::setDefaultAccountId(const QString &accountId) {
    settings_.setValue(QStringLiteral("general/defaultAccountId"), accountId);
    syncAndRestrictPermissions();
}

GeneralSettings SettingsStore::generalSettings() const {
    GeneralSettings g;
    settings_.beginGroup(QStringLiteral("general"));
    g.startMinimized = settings_.value(QStringLiteral("startMinimized"), false).toBool();
    g.closeToTray = settings_.value(QStringLiteral("closeToTray"), false).toBool();
    g.autostart = settings_.value(QStringLiteral("autostart"), false).toBool();
    g.ringtonePath = settings_.value(QStringLiteral("ringtonePath")).toString();
    g.incomingAlert =
        alertModeFromString(settings_.value(QStringLiteral("incomingAlert")).toString());
    settings_.endGroup();
    return g;
}

void SettingsStore::setGeneralSettings(const GeneralSettings &settings) {
    settings_.beginGroup(QStringLiteral("general"));
    settings_.setValue(QStringLiteral("startMinimized"), settings.startMinimized);
    settings_.setValue(QStringLiteral("closeToTray"), settings.closeToTray);
    settings_.setValue(QStringLiteral("autostart"), settings.autostart);
    settings_.setValue(QStringLiteral("ringtonePath"), settings.ringtonePath);
    settings_.setValue(QStringLiteral("incomingAlert"), alertModeToString(settings.incomingAlert));
    settings_.endGroup();
    syncAndRestrictPermissions();
}

AudioSettings SettingsStore::audioSettings() const {
    AudioSettings a;
    settings_.beginGroup(QStringLiteral("audio"));
    a.captureName = settings_.value(QStringLiteral("captureName")).toString();
    a.playbackName = settings_.value(QStringLiteral("playbackName")).toString();
    a.ringName = settings_.value(QStringLiteral("ringName")).toString();
    a.echoCancel = settings_.value(QStringLiteral("echoCancel"), true).toBool();
    a.ecTailMs = settings_.value(QStringLiteral("ecTailMs"), 200).toInt();
    a.codecOrder = settings_.value(QStringLiteral("codecOrder")).toStringList();
    a.disabledCodecs = settings_.value(QStringLiteral("disabledCodecs")).toStringList();
    settings_.endGroup();
    return a;
}

void SettingsStore::setAudioSettings(const AudioSettings &settings) {
    settings_.beginGroup(QStringLiteral("audio"));
    settings_.setValue(QStringLiteral("captureName"), settings.captureName);
    settings_.setValue(QStringLiteral("playbackName"), settings.playbackName);
    settings_.setValue(QStringLiteral("ringName"), settings.ringName);
    settings_.setValue(QStringLiteral("echoCancel"), settings.echoCancel);
    settings_.setValue(QStringLiteral("ecTailMs"), settings.ecTailMs);
    settings_.setValue(QStringLiteral("codecOrder"), settings.codecOrder);
    settings_.setValue(QStringLiteral("disabledCodecs"), settings.disabledCodecs);
    settings_.endGroup();
    syncAndRestrictPermissions();
}

CallSettings SettingsStore::callSettings() const {
    CallSettings c;
    settings_.beginGroup(QStringLiteral("calls"));
    c.dtmfMethod = dtmfFromString(settings_.value(QStringLiteral("dtmfMethod")).toString());
    c.keypadTones = settings_.value(QStringLiteral("keypadTones"), true).toBool();
    c.autoAnswerMode =
        autoAnswerFromString(settings_.value(QStringLiteral("autoAnswerMode")).toString());
    c.autoAnswerDelaySec = settings_.value(QStringLiteral("autoAnswerDelaySec"), 0).toInt();
    c.dnd = settings_.value(QStringLiteral("dnd"), false).toBool();
    c.forwardAlways = settings_.value(QStringLiteral("forwardAlways")).toString();
    c.forwardOnBusy = settings_.value(QStringLiteral("forwardOnBusy")).toString();
    c.forwardOnNoAnswer = settings_.value(QStringLiteral("forwardOnNoAnswer")).toString();
    c.forwardNoAnswerSec = settings_.value(QStringLiteral("forwardNoAnswerSec"), 15).toInt();
    c.recordingsDir = settings_.value(QStringLiteral("recordingsDir")).toString().trimmed();
    if (c.recordingsDir.isEmpty()) {
        c.recordingsDir = AppPaths::recordingsDir();
    }
    c.autoRecord = settings_.value(QStringLiteral("autoRecord"), false).toBool();
    settings_.endGroup();
    return c;
}

void SettingsStore::setCallSettings(const CallSettings &settings) {
    settings_.beginGroup(QStringLiteral("calls"));
    settings_.setValue(QStringLiteral("dtmfMethod"), dtmfToString(settings.dtmfMethod));
    settings_.setValue(QStringLiteral("keypadTones"), settings.keypadTones);
    settings_.setValue(QStringLiteral("autoAnswerMode"), autoAnswerToString(settings.autoAnswerMode));
    settings_.setValue(QStringLiteral("autoAnswerDelaySec"), settings.autoAnswerDelaySec);
    settings_.setValue(QStringLiteral("dnd"), settings.dnd);
    settings_.setValue(QStringLiteral("forwardAlways"), settings.forwardAlways.trimmed());
    settings_.setValue(QStringLiteral("forwardOnBusy"), settings.forwardOnBusy.trimmed());
    settings_.setValue(QStringLiteral("forwardOnNoAnswer"), settings.forwardOnNoAnswer.trimmed());
    settings_.setValue(QStringLiteral("forwardNoAnswerSec"), settings.forwardNoAnswerSec);
    settings_.setValue(QStringLiteral("recordingsDir"), settings.recordingsDir.trimmed());
    settings_.setValue(QStringLiteral("autoRecord"), settings.autoRecord);
    settings_.endGroup();
    syncAndRestrictPermissions();
}

SecretFallbackChoice SettingsStore::secretFallbackChoice() const {
    const QString value = settings_.value(QStringLiteral("secrets/fallback")).toString();
    if (value == QLatin1String("config")) {
        return SecretFallbackChoice::ConfigFile;
    }
    if (value == QLatin1String("declined")) {
        return SecretFallbackChoice::Declined;
    }
    return SecretFallbackChoice::Undecided;
}

void SettingsStore::setSecretFallbackChoice(SecretFallbackChoice choice) {
    const QString key = QStringLiteral("secrets/fallback");
    if (choice == SecretFallbackChoice::Undecided) {
        settings_.remove(key);
    } else {
        settings_.setValue(key, choice == SecretFallbackChoice::ConfigFile
                                    ? QStringLiteral("config")
                                    : QStringLiteral("declined"));
    }
    syncAndRestrictPermissions();
}

WindowState SettingsStore::windowState() const {
    WindowState state;
    state.geometry = settings_.value(QStringLiteral("window/geometry")).toByteArray();
    state.lastTab = settings_.value(QStringLiteral("window/lastTab"), 0).toInt();
    return state;
}

void SettingsStore::setWindowState(const WindowState &state) {
    settings_.setValue(QStringLiteral("window/geometry"), state.geometry);
    settings_.setValue(QStringLiteral("window/lastTab"), state.lastTab);
    syncAndRestrictPermissions();
}

void SettingsStore::syncAndRestrictPermissions() {
    settings_.sync();
    QFile::setPermissions(filePath_, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
}

} // namespace tincan
