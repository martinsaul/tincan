// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include "app/account_service.h"
#include "app/call_service.h"
#include "app/contact_service.h"
#include "app/message_service.h"

#include <QMenu>
#include <QObject>
#include <QString>
#include <QSystemTrayIcon>

#include <memory>

class QAction;

namespace tincan::ui {

class MainWindow;

class TrayIcon : public QObject {
    Q_OBJECT

public:
    TrayIcon(AccountService &accountService, CallService &callService, MainWindow &mainWindow,
             MessageService *messageService = nullptr, ContactService *contactService = nullptr,
             QObject *parent = nullptr);

    void show();
    QSystemTrayIcon &systemTrayIcon();
    QAction *dndAction() const;
    const QString &lastNotificationTitle() const;
    const QString &lastNotificationBody() const;

private:
    void refreshRegistrationState();
    void refreshDndAction(bool enabled);
    void toggleMainWindow();
    void onActivated(QSystemTrayIcon::ActivationReason reason);
    void onMissedCall(const CallInfo &info);
    void onIncomingMessage(const ChatMessage &message, const QString &fromName);
    void postNotification(const QString &title, const QString &body);

    AccountService &accountService_;
    CallService &callService_;
    MainWindow &mainWindow_;
    MessageService *messageService_ = nullptr;
    ContactService *contactService_ = nullptr;
    std::unique_ptr<QMenu> menu_;
    QAction *dndAction_ = nullptr;
    QSystemTrayIcon tray_;
    QString lastNotificationTitle_;
    QString lastNotificationBody_;
};

} // namespace tincan::ui
