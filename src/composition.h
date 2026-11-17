// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include "app/account_service.h"
#include "app/audio_service.h"
#include "app/autostart.h"
#include "app/call_service.h"
#include "app/command_dispatcher.h"
#include "app/command_line.h"
#include "app/config_secret_store.h"
#include "app/contact_repository.h"
#include "app/contact_service.h"
#include "app/database.h"
#include "app/dbus_notifications.h"
#include "app/fallback_secret_store.h"
#include "app/history_repository.h"
#include "app/history_service.h"
#include "app/message_repository.h"
#include "app/message_service.h"
#include "app/notifier.h"
#include "app/secret_store.h"
#include "app/settings_store.h"
#include "app/sip_event_router.h"
#include "sip/pjsip_engine.h"
#include "ui/incoming_call_popup.h"
#include "ui/main_window.h"
#include "ui/tray_icon.h"

#include <QString>

class QDir;

namespace tincan {

class Composition {
public:
    explicit Composition(const QString &settingsPath = {}, const QString &databasePath = {});
    ~Composition();

    Composition(const Composition &) = delete;
    Composition &operator=(const Composition &) = delete;

    bool start(bool nullAudio, QString *error);
    void shutdown();

    bool shouldShowMainWindowOnStart() const;
    void showMainWindow();
    bool saveScreenshots(const QString &directoryPath);

    // Lets the secret store ask the user (modal dialog) about a file fallback.
    void enableInteractivePrompts();
    // Applies the command this process was started with.
    void applyStartupCommand(const CommandLine &command);
    // Applies a command forwarded by a second instance.
    void handleForwardedCommand(const CommandLine &command);

private:
    void wireDesktopIntegration();
    void populateScreenshotData();
    bool saveSettingsScreenshots(const QDir &dir);

    SipEventRouter eventRouter_;
    sip::PjsipEngine engine_;
    SettingsStore settings_;
    KeychainSecretStore keychainSecrets_;
    ConfigSecretStore fileSecrets_;
    FallbackSecretStore secrets_;
    Database database_;
    ContactRepository contactRepository_;
    HistoryRepository historyRepository_;
    MessageRepository messageRepository_;
    AccountService accountService_;
    CallService callService_;
    AudioService audioService_;
    ContactService contactService_;
    HistoryService historyService_;
    MessageService messageService_;
    DbusNotifications notificationBackend_;
    Notifier notifier_;
    CommandDispatcher dispatcher_;
    Autostart autostart_;
    ui::MainWindow mainWindow_;
    ui::IncomingCallPopup incomingPopup_;
    ui::TrayIcon trayIcon_;
};

} // namespace tincan
