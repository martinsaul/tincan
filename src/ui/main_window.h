// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include "app/account_service.h"
#include "app/audio_service.h"
#include "app/call_service.h"
#include "app/secret_store.h"
#include "app/settings_store.h"

#include <QMainWindow>

class QAction;
class QCloseEvent;
class QComboBox;
class QEvent;
class QLabel;
class QTabWidget;
class QToolButton;

namespace tincan::ui {

class CallPanel;
class DialpadWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(AccountService &accountService, CallService &callService, AudioService &audioService,
               SettingsStore &settingsStore, ISecretStore &secretStore, QWidget *parent = nullptr);

    DialpadWidget *dialpad() const;
    CallPanel *callPanel() const;
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
    QWidget *buildHeaderBar();
    QMenu *buildHeaderMenu(QWidget *parent);
    QWidget *createComingSoonPlaceholder() const;
    void refreshAccountsCombo();
    void refreshStatusIndicator();
    void refreshDndState(bool enabled);
    void onAccountComboChanged(int index);
    void onDialRequested(const QString &text);
    void showErrorBanner(const QString &message);

    AccountService &accountService_;
    CallService &callService_;
    AudioService &audioService_;
    SettingsStore &settingsStore_;
    ISecretStore &secretStore_;

    bool quitting_ = false;
    bool updatingCombo_ = false;

    QLabel *statusDot_ = nullptr;
    QComboBox *accountCombo_ = nullptr;
    QLabel *dndBadge_ = nullptr;
    QAction *dndAction_ = nullptr;
    QToolButton *menuButton_ = nullptr;
    QLabel *errorLabel_ = nullptr;
    CallPanel *callPanel_ = nullptr;
    QTabWidget *tabs_ = nullptr;
    DialpadWidget *dialpad_ = nullptr;
};

} // namespace tincan::ui
