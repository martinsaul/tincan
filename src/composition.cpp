// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "composition.h"

#include "core/engine_config.h"
#include "core/version.h"
#include "ui/call_card.h"
#include "ui/call_panel.h"
#include "ui/settings_dialog.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QPixmap>

namespace tincan {

namespace {

EngineConfig buildDefaultEngineConfig(bool nullAudio) {
    EngineConfig cfg;
    cfg.udpPort = 0;
    cfg.tcpPort = 0;
    cfg.nullAudio = nullAudio;
    cfg.userAgent = QStringLiteral("Tincan/") + Version::string();
    return cfg;
}

CallInfo buildSampleConnectedCall() {
    CallInfo sample;
    sample.callId = 900;
    sample.accountId = QStringLiteral("demo");
    sample.direction = CallDirection::Outgoing;
    sample.state = CallState::Confirmed;
    sample.remoteDisplayName = QStringLiteral("Alice Vance");
    sample.remoteUri = QStringLiteral("sip:204@pbx.example.com");
    sample.connectedAt = QDateTime::currentDateTime().addSecs(-83);
    sample.recording = true;
    sample.rxLevel = 1.2f;
    return sample;
}

} // namespace

Composition::Composition(const QString &settingsPath)
    : engine_(eventRouter_), settings_(settingsPath), accountService_(engine_, settings_, secrets_),
      callService_(engine_, accountService_, settings_), audioService_(engine_, settings_),
      mainWindow_(accountService_, callService_, audioService_, settings_, secrets_),
      incomingPopup_(callService_), trayIcon_(accountService_, callService_, mainWindow_) {
    eventRouter_.setTarget(&callService_);
    QObject::connect(&mainWindow_, &ui::MainWindow::closedWithoutTray, &mainWindow_,
                     []() { QCoreApplication::quit(); });
}

Composition::~Composition() {
    shutdown();
}

bool Composition::start(bool nullAudio, QString *error) {
    if (!engine_.start(buildDefaultEngineConfig(nullAudio), error)) {
        return false;
    }
    audioService_.applyCurrentSettings();
    accountService_.load();
    trayIcon_.show();
    return true;
}

void Composition::shutdown() {
    eventRouter_.setTarget(nullptr);
    engine_.shutdown();
}

bool Composition::shouldShowMainWindowOnStart() const {
    return !settings_.generalSettings().startMinimized;
}

void Composition::showMainWindow() {
    mainWindow_.show();
}

bool Composition::saveScreenshots(const QString &directoryPath) {
    QDir dir(directoryPath);
    if (!dir.mkpath(QStringLiteral("."))) {
        return false;
    }

    mainWindow_.show();
    QCoreApplication::processEvents();
    const bool mainOk = mainWindow_.grab().save(dir.filePath(QStringLiteral("main_window.png")));

    CallInfo sample = buildSampleConnectedCall();
    callService_.onCallChanged(sample);
    QCoreApplication::processEvents();
    bool cardOk = false;
    if (ui::CallCard *card = mainWindow_.callPanel()->cardForCall(sample.callId)) {
        card->setKeypadExpanded(true);
        card->appendDtmfEcho(QStringLiteral("120#"));
        mainWindow_.resize(320, 680);
        QCoreApplication::processEvents();
        cardOk = card->grab().save(dir.filePath(QStringLiteral("call_card.png")));
        (void)mainWindow_.grab().save(dir.filePath(QStringLiteral("main_window_in_call.png")));
    }
    sample.state = CallState::Disconnected;
    callService_.onCallChanged(sample);
    QCoreApplication::processEvents();

    ui::SettingsDialog settingsDlg(accountService_, audioService_, settings_, secrets_);
    settingsDlg.show();
    QCoreApplication::processEvents();
    const bool settingsOk =
        settingsDlg.grab().save(dir.filePath(QStringLiteral("settings_dialog.png")));

    settingsDlg.setCurrentPageIndex(1);
    QCoreApplication::processEvents();
    (void)settingsDlg.grab().save(dir.filePath(QStringLiteral("settings_audio.png")));

    settingsDlg.setCurrentPageIndex(2);
    QCoreApplication::processEvents();
    (void)settingsDlg.grab().save(dir.filePath(QStringLiteral("settings_general.png")));

    settingsDlg.setCurrentPageIndex(3);
    QCoreApplication::processEvents();
    (void)settingsDlg.grab().save(dir.filePath(QStringLiteral("settings_calls.png")));

    return mainOk && cardOk && settingsOk;
}

} // namespace tincan
