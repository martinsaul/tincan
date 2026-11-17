// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "composition.h"

#include "core/engine_config.h"
#include "core/version.h"
#include "ui/settings_dialog.h"

#include <QCoreApplication>
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

    return mainOk && settingsOk;
}

} // namespace tincan
