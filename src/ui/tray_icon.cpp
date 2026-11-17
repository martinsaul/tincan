// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "ui/tray_icon.h"

#include "ui/icons.h"
#include "ui/main_window.h"

#include <QAction>

namespace tincan::ui {

TrayIcon::TrayIcon(AccountService &accountService, CallService &callService, MainWindow &mainWindow,
                   QObject *parent)
    : QObject(parent), accountService_(accountService), callService_(callService),
      mainWindow_(mainWindow), menu_(std::make_unique<QMenu>()) {
    QAction *showHideAction = menu_->addAction(tr("Show / Hide"));
    QAction *settingsAction = menu_->addAction(Icons::settings(), tr("Settings..."));
    menu_->addSeparator();
    QAction *quitAction = menu_->addAction(tr("Quit"));

    connect(showHideAction, &QAction::triggered, this, &TrayIcon::toggleMainWindow);
    connect(settingsAction, &QAction::triggered, &mainWindow_, &MainWindow::openSettingsDialog);
    connect(quitAction, &QAction::triggered, &mainWindow_, &MainWindow::requestQuit);

    tray_.setContextMenu(menu_.get());
    connect(&tray_, &QSystemTrayIcon::activated, this, &TrayIcon::onActivated);

    connect(&accountService_, &AccountService::registrationChanged, this,
            [this](const RegistrationInfo &) { refreshRegistrationState(); });
    connect(&accountService_, &AccountService::defaultAccountChanged, this,
            [this](const QString &) { refreshRegistrationState(); });
    connect(&accountService_, &AccountService::accountsChanged, this,
            &TrayIcon::refreshRegistrationState);
    connect(&callService_, &CallService::missedCall, this, &TrayIcon::onMissedCall);

    refreshRegistrationState();
}

void TrayIcon::show() {
    if (QSystemTrayIcon::isSystemTrayAvailable()) {
        tray_.show();
    }
}

QSystemTrayIcon &TrayIcon::systemTrayIcon() {
    return tray_;
}

void TrayIcon::refreshRegistrationState() {
    const QString defId = accountService_.defaultAccountId();
    const RegistrationInfo reg = defId.isEmpty() ? RegistrationInfo{}
                                                 : accountService_.registrationInfo(defId);
    tray_.setIcon(Icons::trayIconForState(reg.state));

    QString stateText = tr("Unregistered");
    switch (reg.state) {
    case RegState::Unregistered:
        stateText = tr("Unregistered");
        break;
    case RegState::Registering:
        stateText = tr("Registering...");
        break;
    case RegState::Registered:
        stateText = tr("Registered");
        break;
    case RegState::Failed:
        stateText = tr("Registration failed");
        break;
    }
    tray_.setToolTip(tr("Tincan — %1").arg(stateText));
}

void TrayIcon::toggleMainWindow() {
    if (mainWindow_.isVisible() && !mainWindow_.isMinimized()) {
        mainWindow_.hide();
    } else {
        mainWindow_.showNormal();
        mainWindow_.raise();
        mainWindow_.activateWindow();
    }
}

void TrayIcon::onActivated(QSystemTrayIcon::ActivationReason reason) {
    if (reason == QSystemTrayIcon::Trigger) {
        toggleMainWindow();
    }
}

void TrayIcon::onMissedCall(const CallInfo &info) {
    const QString caller =
        info.remoteDisplayName.trimmed().isEmpty() ? info.remoteUri : info.remoteDisplayName;
    tray_.showMessage(tr("Missed call"), tr("Missed call from %1").arg(caller), Icons::appIcon());
}

} // namespace tincan::ui
