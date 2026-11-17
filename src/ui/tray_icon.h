// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include "app/account_service.h"
#include "app/call_service.h"

#include <QMenu>
#include <QObject>
#include <QSystemTrayIcon>

#include <memory>

namespace tincan::ui {

class MainWindow;

class TrayIcon : public QObject {
    Q_OBJECT

public:
    TrayIcon(AccountService &accountService, CallService &callService, MainWindow &mainWindow,
             QObject *parent = nullptr);

    void show();
    QSystemTrayIcon &systemTrayIcon();

private:
    void refreshRegistrationState();
    void toggleMainWindow();
    void onActivated(QSystemTrayIcon::ActivationReason reason);
    void onMissedCall(const CallInfo &info);

    AccountService &accountService_;
    CallService &callService_;
    MainWindow &mainWindow_;
    std::unique_ptr<QMenu> menu_;
    QSystemTrayIcon tray_;
};

} // namespace tincan::ui
