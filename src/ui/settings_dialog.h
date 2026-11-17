// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include "app/account_service.h"
#include "app/audio_service.h"
#include "app/secret_store.h"
#include "app/settings_store.h"

#include <QDialog>

class QListWidget;
class QStackedWidget;

namespace tincan::ui {

class AccountsPage;
class AudioPage;
class CallsPage;
class GeneralPage;

class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    SettingsDialog(AccountService &accountService, AudioService &audioService,
                   SettingsStore &settingsStore, ISecretStore &secretStore,
                   QWidget *parent = nullptr);
    ~SettingsDialog() override;

    void setCurrentPageIndex(int index);
    AccountsPage *accountsPage() const;
    AudioPage *audioPage() const;
    GeneralPage *generalPage() const;
    CallsPage *callsPage() const;

    void accept() override;
    void reject() override;

private:
    QListWidget *navList_ = nullptr;
    QStackedWidget *stack_ = nullptr;
    AccountsPage *accountsPage_ = nullptr;
    AudioPage *audioPage_ = nullptr;
    GeneralPage *generalPage_ = nullptr;
    CallsPage *callsPage_ = nullptr;
};

} // namespace tincan::ui
