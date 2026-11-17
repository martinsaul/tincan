// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include "app/account_service.h"
#include "app/audio_service.h"
#include "app/call_service.h"
#include "app/contact_service.h"
#include "app/history_service.h"
#include "app/message_service.h"
#include "app/secret_store.h"
#include "app/settings_store.h"

#include <QMainWindow>

class QAction;
class QCloseEvent;
class QComboBox;
class QEvent;
class QLabel;
class QTabWidget;

namespace tincan::ui {

class CallPanel;
class ContactsView;
class DialpadWidget;
class HeaderBar;
class HistoryView;
class MessagesView;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(AccountService &accountService, CallService &callService, AudioService &audioService,
               ContactService &contactService, HistoryService &historyService,
               MessageService &messageService, SettingsStore &settingsStore,
               ISecretStore &secretStore, QWidget *parent = nullptr);

    DialpadWidget *dialpad() const;
    CallPanel *callPanel() const;
    ContactsView *contactsView() const;
    HistoryView *historyView() const;
    MessagesView *messagesView() const;
    QTabWidget *tabs() const;
    QComboBox *presenceCombo() const;
    QLabel *dndBadge() const;
    QAction *dndAction() const;

public slots:
    void openSettingsDialog();
    void requestQuit();

signals:
    void closedWithoutTray();

protected:
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void setupTabs(QWidget *central);
    void connectSignals();
    void refreshHistoryTabBadge();
    void refreshMessagesTabBadge();
    void onTabChanged(int index);
    void onDialRequested(const QString &text);
    void onDialOnAccountRequested(const QString &target, const QString &accountId);
    void showErrorBanner(const QString &message);

    AccountService &accountService_;
    CallService &callService_;
    AudioService &audioService_;
    ContactService &contactService_;
    HistoryService &historyService_;
    MessageService &messageService_;
    SettingsStore &settingsStore_;
    ISecretStore &secretStore_;

    bool quitting_ = false;
    int historyTabIndex_ = -1;
    int messagesTabIndex_ = -1;

    HeaderBar *headerBar_ = nullptr;
    QLabel *errorLabel_ = nullptr;
    CallPanel *callPanel_ = nullptr;
    QTabWidget *tabs_ = nullptr;
    DialpadWidget *dialpad_ = nullptr;
    ContactsView *contactsView_ = nullptr;
    HistoryView *historyView_ = nullptr;
    MessagesView *messagesView_ = nullptr;
};

} // namespace tincan::ui
