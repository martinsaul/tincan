// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "ui/accounts_page.h"

#include "ui/account_dialog.h"

#include <QHBoxLayout>
#include <QListWidget>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QVBoxLayout>

namespace tincan::ui {

namespace {

QString formatAccountEntry(const AccountConfig &acc, bool isDefault) {
    const QString identity = acc.user.trimmed().isEmpty()
                                 ? acc.domain
                                 : QStringLiteral("%1@%2").arg(acc.user, acc.domain);
    const QString base = acc.displayName.trimmed().isEmpty()
                             ? identity
                             : QStringLiteral("%1 (%2)").arg(acc.displayName, identity);
    return isDefault ? QObject::tr("%1  [Default]").arg(base) : base;
}

} // namespace

AccountsPage::AccountsPage(AccountService &accountService, ISecretStore &secretStore,
                           QWidget *parent)
    : QWidget(parent), accountService_(accountService), secretStore_(secretStore) {
    setObjectName(QStringLiteral("accountsPage"));
    auto *root = new QVBoxLayout(this);

    listWidget_ = new QListWidget(this);
    listWidget_->setObjectName(QStringLiteral("accountsListWidget"));
    connect(listWidget_, &QListWidget::itemSelectionChanged, this,
            &AccountsPage::updateButtonStates);
    connect(listWidget_, &QListWidget::itemDoubleClicked, this,
            [this](QListWidgetItem *) { onEditClicked(); });
    root->addWidget(listWidget_, 1);

    auto *btnRow = new QHBoxLayout();
    addButton_ = new QPushButton(tr("Add..."), this);
    addButton_->setObjectName(QStringLiteral("addAccountButton"));
    editButton_ = new QPushButton(tr("Edit..."), this);
    editButton_->setObjectName(QStringLiteral("editAccountButton"));
    removeButton_ = new QPushButton(tr("Remove"), this);
    removeButton_->setObjectName(QStringLiteral("removeAccountButton"));
    makeDefaultButton_ = new QPushButton(tr("Make Default"), this);
    makeDefaultButton_->setObjectName(QStringLiteral("makeDefaultButton"));

    connect(addButton_, &QPushButton::clicked, this, &AccountsPage::onAddClicked);
    connect(editButton_, &QPushButton::clicked, this, &AccountsPage::onEditClicked);
    connect(removeButton_, &QPushButton::clicked, this, &AccountsPage::onRemoveClicked);
    connect(makeDefaultButton_, &QPushButton::clicked, this, &AccountsPage::onMakeDefaultClicked);

    btnRow->addWidget(addButton_);
    btnRow->addWidget(editButton_);
    btnRow->addWidget(removeButton_);
    btnRow->addWidget(makeDefaultButton_);
    btnRow->addStretch();
    root->addLayout(btnRow);

    connect(&accountService_, &AccountService::accountsChanged, this, &AccountsPage::refreshList);
    connect(&accountService_, &AccountService::defaultAccountChanged, this,
            [this](const QString &) { refreshList(); });

    refreshList();
}

void AccountsPage::refreshList() {
    const QString prevSelected = selectedAccountId();
    listWidget_->clear();

    const QString defaultId = accountService_.defaultAccountId();
    for (const AccountConfig &acc : accountService_.accounts()) {
        auto *item =
            new QListWidgetItem(formatAccountEntry(acc, acc.id == defaultId), listWidget_);
        item->setData(Qt::UserRole, acc.id);
        if (acc.id == prevSelected) {
            listWidget_->setCurrentItem(item);
        }
    }
    if (listWidget_->currentRow() < 0 && listWidget_->count() > 0) {
        listWidget_->setCurrentRow(0);
    }
    updateButtonStates();
}

void AccountsPage::updateButtonStates() {
    const QString selId = selectedAccountId();
    const bool hasSel = !selId.isEmpty();
    editButton_->setEnabled(hasSel);
    removeButton_->setEnabled(hasSel);
    makeDefaultButton_->setEnabled(hasSel && selId != accountService_.defaultAccountId());
}

QString AccountsPage::selectedAccountId() const {
    const QListWidgetItem *item = listWidget_->currentItem();
    return item != nullptr ? item->data(Qt::UserRole).toString() : QString{};
}

void AccountsPage::onAddClicked() {
    AccountDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        QString err;
        if (!accountService_.addAccount(dlg.accountConfig(), &err) && !err.isEmpty()) {
            QMessageBox::warning(this, tr("Account Error"), err);
        }
    }
}

void AccountsPage::onEditClicked() {
    const QString id = selectedAccountId();
    const auto existing = accountService_.account(id);
    if (!existing.has_value()) {
        return;
    }

    AccountDialog dlg(*existing, this);
    QPointer<AccountDialog> safeDlg(&dlg);
    secretStore_.readPassword(id, [safeDlg](const QString &pw, const QString &) {
        if (safeDlg && !pw.isEmpty()) {
            safeDlg->setPasswordFieldIfUnmodified(pw);
        }
    });

    if (dlg.exec() == QDialog::Accepted) {
        QString err;
        if (!accountService_.updateAccount(dlg.accountConfig(), &err) && !err.isEmpty()) {
            QMessageBox::warning(this, tr("Account Error"), err);
        }
    }
}

void AccountsPage::onRemoveClicked() {
    const QString id = selectedAccountId();
    if (!id.isEmpty()) {
        accountService_.removeAccount(id);
    }
}

void AccountsPage::onMakeDefaultClicked() {
    const QString id = selectedAccountId();
    if (!id.isEmpty()) {
        accountService_.setDefaultAccountId(id);
    }
}

} // namespace tincan::ui
