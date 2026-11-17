// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "composition.h"

#include "core/engine_config.h"
#include "core/version.h"
#include "ui/messages_view.h"
#include "ui/secret_fallback_prompt.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

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

Database &openDbOnInit(Database &db) {
    (void)db.open();
    return db;
}

QString secretsFilePath(const SettingsStore &settings) {
    return QFileInfo(settings.filePath()).dir().filePath(QStringLiteral("secrets.ini"));
}

} // namespace

Composition::Composition(const QString &settingsPath, const QString &databasePath)
    : engine_(eventRouter_), settings_(settingsPath), fileSecrets_(secretsFilePath(settings_)),
      secrets_(keychainSecrets_, fileSecrets_, settings_), database_(databasePath),
      contactRepository_(openDbOnInit(database_)), historyRepository_(database_),
      messageRepository_(database_), accountService_(engine_, settings_, secrets_),
      callService_(engine_, accountService_, settings_), audioService_(engine_, settings_),
      contactService_(engine_, accountService_, contactRepository_),
      historyService_(callService_, historyRepository_, &contactService_),
      messageService_(engine_, accountService_, messageRepository_),
      notifier_(callService_, settings_, notificationBackend_, &messageService_, &contactService_),
      dispatcher_(callService_),
      mainWindow_(accountService_, callService_, audioService_, contactService_, historyService_,
                  messageService_, settings_, secrets_),
      incomingPopup_(callService_, &contactService_),
      trayIcon_(accountService_, callService_, mainWindow_, &messageService_, &contactService_) {
    eventRouter_.addListener(&callService_);
    eventRouter_.addListener(&contactService_);
    eventRouter_.addListener(&messageService_);
    QObject::connect(&mainWindow_, &ui::MainWindow::closedWithoutTray, &mainWindow_,
                     []() { QCoreApplication::quit(); });
    wireDesktopIntegration();
}

void Composition::wireDesktopIntegration() {
    trayIcon_.setBuiltInNotificationsEnabled(false);
    incomingPopup_.setShowPredicate([this]() {
        return settings_.generalSettings().incomingAlert != IncomingAlertMode::Notification;
    });
    notifier_.setMessageSuppressor([this](const ChatMessage &message) {
        return mainWindow_.messagesView() != nullptr &&
               mainWindow_.messagesView()->isPeerChatVisible(message.peerUri);
    });
    QObject::connect(&notifier_, &Notifier::fallbackRequested, &trayIcon_,
                     &ui::TrayIcon::showNotification);
    QObject::connect(&notifier_, &Notifier::openHistoryRequested, &mainWindow_,
                     &ui::MainWindow::showHistoryTab);
    QObject::connect(&notifier_, &Notifier::openChatRequested, &mainWindow_,
                     [this](const QString &peerUri, const QString &accountId) {
                         mainWindow_.bringToFront();
                         (void)mainWindow_.messagesView()->openChatForPeer(peerUri, accountId);
                     });
    QObject::connect(&dispatcher_, &CommandDispatcher::showRequested, &mainWindow_,
                     &ui::MainWindow::bringToFront);
    QObject::connect(&dispatcher_, &CommandDispatcher::quitRequested, &mainWindow_,
                     &ui::MainWindow::requestQuit);
    QObject::connect(&mainWindow_, &ui::MainWindow::settingsApplied, &mainWindow_,
                     [this]() { (void)autostart_.apply(settings_.generalSettings().autostart); });
}

void Composition::enableInteractivePrompts() {
    secrets_.setConsentPrompt([this]() { return ui::SecretFallbackPrompt::ask(&mainWindow_); });
}

void Composition::applyStartupCommand(const CommandLine &command) {
    const bool startHidden = command.action == CommandLine::Action::Minimized ||
                             command.action == CommandLine::Action::Quit ||
                             !shouldShowMainWindowOnStart();
    if (!startHidden) {
        showMainWindow();
    }
    (void)dispatcher_.dispatch(command);
}

void Composition::handleForwardedCommand(const CommandLine &command) {
    if (command.action == CommandLine::Action::None) {
        // Launching Tincan again (e.g. from the menu) brings the window back.
        mainWindow_.bringToFront();
        return;
    }
    (void)dispatcher_.dispatch(command);
}

Composition::~Composition() {
    shutdown();
}

bool Composition::start(bool nullAudio, QString *error) {
    if (!database_.open(error)) {
        return false;
    }
    if (!engine_.start(buildDefaultEngineConfig(nullAudio), error)) {
        return false;
    }
    audioService_.applyCurrentSettings();
    accountService_.load();
    trayIcon_.show();
    return true;
}

void Composition::shutdown() {
    eventRouter_.clear();
    engine_.shutdown();
    database_.close();
}

bool Composition::shouldShowMainWindowOnStart() const {
    return !settings_.generalSettings().startMinimized;
}

void Composition::showMainWindow() { mainWindow_.show(); }

} // namespace tincan
