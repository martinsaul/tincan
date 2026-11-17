// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include "app/account_service.h"
#include "app/audio_service.h"
#include "app/call_service.h"
#include "app/secret_store.h"
#include "app/settings_store.h"
#include "app/sip_event_router.h"
#include "sip/pjsip_engine.h"
#include "ui/incoming_call_popup.h"
#include "ui/main_window.h"
#include "ui/tray_icon.h"

#include <QString>

namespace tincan {

class Composition {
public:
    explicit Composition(const QString &settingsPath = {});
    ~Composition();

    Composition(const Composition &) = delete;
    Composition &operator=(const Composition &) = delete;

    bool start(bool nullAudio, QString *error);
    void shutdown();

    bool shouldShowMainWindowOnStart() const;
    void showMainWindow();
    bool saveScreenshots(const QString &directoryPath);

private:
    SipEventRouter eventRouter_;
    sip::PjsipEngine engine_;
    SettingsStore settings_;
    KeychainSecretStore secrets_;
    AccountService accountService_;
    CallService callService_;
    AudioService audioService_;
    ui::MainWindow mainWindow_;
    ui::IncomingCallPopup incomingPopup_;
    ui::TrayIcon trayIcon_;
};

} // namespace tincan
