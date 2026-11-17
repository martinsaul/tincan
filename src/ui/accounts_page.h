// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include "app/account_service.h"
#include "app/secret_store.h"

#include <QWidget>

class QListWidget;
class QPushButton;

namespace tincan::ui {

class AccountsPage : public QWidget {
    Q_OBJECT

public:
    AccountsPage(AccountService &accountService, ISecretStore &secretStore,
                 QWidget *parent = nullptr);

private:
    void refreshList();
    void updateButtonStates();
    QString selectedAccountId() const;
    void onAddClicked();
    void onEditClicked();
    void onRemoveClicked();
    void onMakeDefaultClicked();

    AccountService &accountService_;
    ISecretStore &secretStore_;
    QListWidget *listWidget_ = nullptr;
    QPushButton *addButton_ = nullptr;
    QPushButton *editButton_ = nullptr;
    QPushButton *removeButton_ = nullptr;
    QPushButton *makeDefaultButton_ = nullptr;
};

} // namespace tincan::ui
