// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "composition.h"

#include "core/engine_config.h"
#include "core/version.h"
#include "ui/call_card.h"
#include "ui/call_panel.h"
#include "ui/chat_window.h"
#include "ui/messages_view.h"
#include "ui/settings_dialog.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QPixmap>
#include <QTabWidget>

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

Database &openDbOnInit(Database &db) {
    (void)db.open();
    return db;
}

} // namespace

Composition::Composition(const QString &settingsPath, const QString &databasePath)
    : engine_(eventRouter_), settings_(settingsPath), database_(databasePath),
      contactRepository_(openDbOnInit(database_)), historyRepository_(database_),
      messageRepository_(database_), accountService_(engine_, settings_, secrets_),
      callService_(engine_, accountService_, settings_), audioService_(engine_, settings_),
      contactService_(engine_, accountService_, contactRepository_),
      historyService_(callService_, historyRepository_, &contactService_),
      messageService_(engine_, accountService_, messageRepository_),
      mainWindow_(accountService_, callService_, audioService_, contactService_, historyService_,
                  messageService_, settings_, secrets_),
      incomingPopup_(callService_, &contactService_),
      trayIcon_(accountService_, callService_, mainWindow_, &messageService_, &contactService_) {
    eventRouter_.addListener(&callService_);
    eventRouter_.addListener(&contactService_);
    eventRouter_.addListener(&messageService_);
    QObject::connect(&mainWindow_, &ui::MainWindow::closedWithoutTray, &mainWindow_,
                     []() { QCoreApplication::quit(); });
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

void Composition::populateScreenshotData() {
    AccountConfig demoAcc;
    demoAcc.id = QStringLiteral("demo");
    demoAcc.displayName = QStringLiteral("Office PBX");
    demoAcc.user = QStringLiteral("100");
    demoAcc.domain = QStringLiteral("pbx.example.com");
    demoAcc.registerAccount = false;
    demoAcc.publishPresence = true;
    (void)accountService_.addAccount(demoAcc);
    accountService_.handleRegistrationChanged(
        RegistrationInfo{demoAcc.id, RegState::Registered, 200, QStringLiteral("OK")});

    (void)contactService_.addContact(
        Contact{0, QStringLiteral("Alice Vance"), QStringLiteral("204"), demoAcc.id, true, {}, {}, {}});
    (void)contactService_.addContact(
        Contact{0, QStringLiteral("Bob Chen"), QStringLiteral("210"), demoAcc.id, true, {}, {}, {}});
    (void)contactService_.addContact(
        Contact{0, QStringLiteral("Clara Rivera"), QStringLiteral("305"), demoAcc.id, true, {}, {}, {}});
    (void)contactService_.addContact(
        Contact{0, QStringLiteral("Helpdesk"), QStringLiteral("500"), demoAcc.id, false, {}, {}, {}});

    contactService_.onPresenceChanged(PresenceInfo{
        QStringLiteral("sip:204@pbx.example.com"), PresenceState::Online, QStringLiteral("Online")});
    contactService_.onPresenceChanged(PresenceInfo{QStringLiteral("sip:210@pbx.example.com"),
                                                   PresenceState::OnThePhone,
                                                   QStringLiteral("On the phone")});
    contactService_.onPresenceChanged(PresenceInfo{
        QStringLiteral("sip:305@pbx.example.com"), PresenceState::Away, QStringLiteral("Away")});

    const QDateTime now = QDateTime::currentDateTimeUtc();
    (void)historyRepository_.insert(HistoryEntry{
        0, demoAcc.id, CallDirection::Incoming, QStringLiteral("sip:210@pbx.example.com"),
        QStringLiteral("Bob Chen"), now.addSecs(-900), {}, now.addSecs(-885), 487, true, false});
    (void)historyRepository_.insert(HistoryEntry{
        0, demoAcc.id, CallDirection::Incoming, QStringLiteral("sip:305@pbx.example.com"),
        QStringLiteral("Clara Rivera"), now.addSecs(-3600), now.addSecs(-3592), now.addSecs(-3350),
        200, false, true});

    const qint64 sentId = messageRepository_.insert(ChatMessage{
        0, demoAcc.id, QStringLiteral("sip:204@pbx.example.com"), MessageDirection::Outgoing,
        QStringLiteral("Hi Alice, are you free for a quick sync?"), now.addSecs(-300),
        MessageStatus::Sent, true});
    (void)sentId;
    messageService_.onIncomingMessage(MessageInfo{
        demoAcc.id, QStringLiteral("sip:204@pbx.example.com"), QStringLiteral("Alice Vance"),
        QStringLiteral("Sure! Give me two minutes and dial ext 204."),
        QStringLiteral("text/plain"), now.addSecs(-120)});
    messageService_.onIncomingMessage(MessageInfo{
        demoAcc.id, QStringLiteral("sip:210@pbx.example.com"), QStringLiteral("Bob Chen"),
        QStringLiteral("Left the deployment notes in the shared folder."),
        QStringLiteral("text/plain"), now.addSecs(-60)});
}

bool Composition::saveScreenshots(const QString &directoryPath) {
    QDir dir(directoryPath);
    if (!dir.mkpath(QStringLiteral("."))) {
        return false;
    }

    populateScreenshotData();

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
    sample.lastStatusCode = 200;
    callService_.onCallChanged(sample);
    mainWindow_.resize(320, 560);
    QCoreApplication::processEvents();

    mainWindow_.tabs()->setCurrentIndex(1);
    QCoreApplication::processEvents();
    contactService_.onPresenceChanged(PresenceInfo{
        QStringLiteral("sip:204@pbx.example.com"), PresenceState::Online, QStringLiteral("Online")});
    contactService_.onPresenceChanged(PresenceInfo{QStringLiteral("sip:210@pbx.example.com"),
                                                   PresenceState::OnThePhone,
                                                   QStringLiteral("On the phone")});
    contactService_.onPresenceChanged(PresenceInfo{
        QStringLiteral("sip:305@pbx.example.com"), PresenceState::Away, QStringLiteral("Away")});
    QCoreApplication::processEvents();
    const bool contactsOk = mainWindow_.grab().save(dir.filePath(QStringLiteral("contacts.png")));

    mainWindow_.tabs()->setCurrentIndex(2);
    QCoreApplication::processEvents();
    const bool historyOk = mainWindow_.grab().save(dir.filePath(QStringLiteral("history.png")));

    mainWindow_.tabs()->setCurrentIndex(3);
    QCoreApplication::processEvents();
    (void)mainWindow_.grab().save(dir.filePath(QStringLiteral("messages.png")));

    ui::ChatWindow *chatWin =
        mainWindow_.messagesView()->openChatForPeer(QStringLiteral("204"), QStringLiteral("demo"));
    bool chatOk = false;
    if (chatWin != nullptr) {
        QCoreApplication::processEvents();
        chatOk = chatWin->grab().save(dir.filePath(QStringLiteral("chat_window.png")));
        chatWin->close();
    }

    const bool settingsOk = saveSettingsScreenshots(dir);
    return mainOk && cardOk && contactsOk && historyOk && chatOk && settingsOk;
}

bool Composition::saveSettingsScreenshots(const QDir &dir) {
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
    return settingsOk;
}

} // namespace tincan
