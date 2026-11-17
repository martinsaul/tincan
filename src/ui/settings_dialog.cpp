// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "ui/settings_dialog.h"

#include "ui/accounts_page.h"
#include "ui/audio_page.h"
#include "ui/calls_page.h"
#include "ui/general_page.h"
#include "ui/icons.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QListWidget>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace tincan::ui {

SettingsDialog::SettingsDialog(AccountService &accountService, AudioService &audioService,
                               SettingsStore &settingsStore, ISecretStore &secretStore,
                               QWidget *parent)
    : QDialog(parent) {
    setObjectName(QStringLiteral("settingsDialog"));
    setWindowTitle(tr("Settings — Tincan"));
    setWindowIcon(Icons::appIcon());
    resize(580, 470);

    auto *root = new QVBoxLayout(this);

    auto *body = new QHBoxLayout();
    navList_ = new QListWidget(this);
    navList_->setObjectName(QStringLiteral("settingsNavList"));
    navList_->setFixedWidth(135);
    navList_->addItem(tr("Accounts"));
    navList_->addItem(tr("Audio"));
    navList_->addItem(tr("General"));
    navList_->addItem(tr("Calls"));

    stack_ = new QStackedWidget(this);
    accountsPage_ = new AccountsPage(accountService, secretStore, stack_);
    audioPage_ = new AudioPage(audioService, stack_);
    generalPage_ = new GeneralPage(settingsStore, audioService, stack_);
    callsPage_ = new CallsPage(settingsStore, stack_);
    stack_->addWidget(accountsPage_);
    stack_->addWidget(audioPage_);
    stack_->addWidget(generalPage_);
    stack_->addWidget(callsPage_);

    connect(navList_, &QListWidget::currentRowChanged, stack_, &QStackedWidget::setCurrentIndex);
    navList_->setCurrentRow(0);

    body->addWidget(navList_);
    body->addWidget(stack_, 1);
    root->addLayout(body, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &SettingsDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &SettingsDialog::reject);
    root->addWidget(buttons);
}

SettingsDialog::~SettingsDialog() {
    generalPage_->stopPreview();
}

void SettingsDialog::setCurrentPageIndex(int index) {
    if (index >= 0 && index < navList_->count()) {
        navList_->setCurrentRow(index);
    }
}

AccountsPage *SettingsDialog::accountsPage() const { return accountsPage_; }
AudioPage *SettingsDialog::audioPage() const { return audioPage_; }
GeneralPage *SettingsDialog::generalPage() const { return generalPage_; }
CallsPage *SettingsDialog::callsPage() const { return callsPage_; }

void SettingsDialog::accept() {
    audioPage_->applyChanges();
    generalPage_->applyChanges();
    callsPage_->applyChanges();
    generalPage_->stopPreview();
    QDialog::accept();
}

void SettingsDialog::reject() {
    generalPage_->stopPreview();
    QDialog::reject();
}

} // namespace tincan::ui
