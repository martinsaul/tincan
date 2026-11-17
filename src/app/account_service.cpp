// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "app/account_service.h"

#include <QPointer>
#include <QUuid>

namespace tincan {

namespace {

void setOutError(QString *error, const QString &message) {
    if (error != nullptr) {
        *error = message;
    }
}

} // namespace

AccountService::AccountService(ISipEngine &engine, SettingsStore &settings, ISecretStore &secrets,
                               QObject *parent)
    : QObject(parent), engine_(engine), settings_(settings), secrets_(secrets) {}

void AccountService::load() {
    for (const AccountConfig &existing : accounts_) {
        engine_.removeAccount(existing.id);
    }

    accounts_ = settings_.accounts();
    defaultAccountId_ = settings_.defaultAccountId();
    registrations_.clear();
    normalizeDefaultAccountId();

    for (const AccountConfig &acc : accounts_) {
        registrations_.insert(acc.id, RegistrationInfo{acc.id, RegState::Unregistered, 0, {}});
        const quint64 rev = ++accountRevisions_[acc.id];
        QPointer<AccountService> self(this);
        secrets_.readPassword(acc.id,
                              [self, id = acc.id, rev](const QString &password, const QString &) {
                                  if (!self || self->accountRevisions_.value(id) != rev) {
                                      return;
                                  }
                                  const qsizetype idx = self->findAccountIndex(id);
                                  if (idx < 0) {
                                      return;
                                  }
                                  self->accounts_[idx].password = password;
                                  self->engine_.removeAccount(id);
                                  QString err;
                                  if (self->engine_.addAccount(self->accounts_[idx], &err)) {
                                      self->applyOwnPresenceToAccount(id);
                                  }
                              });
    }

    emit accountsChanged();
}

QList<AccountConfig> AccountService::accounts() const { return accounts_; }

std::optional<AccountConfig> AccountService::account(const QString &accountId) const {
    const qsizetype idx = findAccountIndex(accountId);
    return idx < 0 ? std::nullopt : std::optional<AccountConfig>{accounts_[idx]};
}

std::optional<AccountConfig> AccountService::defaultAccount() const {
    return defaultAccountId_.isEmpty() ? std::nullopt : account(defaultAccountId_);
}

QString AccountService::defaultAccountId() const { return defaultAccountId_; }

void AccountService::setDefaultAccountId(const QString &accountId) {
    if (accountId == defaultAccountId_ || findAccountIndex(accountId) < 0) {
        return;
    }
    defaultAccountId_ = accountId;
    settings_.setDefaultAccountId(defaultAccountId_);
    applyOwnPresenceToAccount(defaultAccountId_);
    emit defaultAccountChanged(defaultAccountId_);
    emit accountsChanged();
}

bool AccountService::addAccount(AccountConfig config, QString *error) {
    if (config.id.trimmed().isEmpty()) {
        config.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    }
    if (config.domain.trimmed().isEmpty()) {
        setOutError(error, tr("Domain is required."));
        return false;
    }
    if (findAccountIndex(config.id) >= 0) {
        setOutError(error, tr("An account with this ID already exists."));
        return false;
    }

    ++accountRevisions_[config.id];
    accounts_.append(config);
    registrations_.insert(config.id, RegistrationInfo{config.id, RegState::Unregistered, 0, {}});

    const bool becameDefault = defaultAccountId_.isEmpty();
    if (becameDefault) {
        defaultAccountId_ = config.id;
        settings_.setDefaultAccountId(defaultAccountId_);
    }

    settings_.setAccounts(accounts_);
    secrets_.writePassword(config.id, config.password);
    const bool ok = engine_.addAccount(config, error);
    if (ok) {
        applyOwnPresenceToAccount(config.id);
    }

    emit accountsChanged();
    if (becameDefault) {
        emit defaultAccountChanged(defaultAccountId_);
    }
    return ok;
}

bool AccountService::updateAccount(const AccountConfig &config, QString *error) {
    const qsizetype idx = findAccountIndex(config.id);
    if (idx < 0) {
        setOutError(error, tr("Account not found."));
        return false;
    }
    if (config.domain.trimmed().isEmpty()) {
        setOutError(error, tr("Domain is required."));
        return false;
    }

    ++accountRevisions_[config.id];
    const QString keptPassword = accounts_[idx].password;
    accounts_[idx] = config;
    if (config.password.isEmpty()) {
        accounts_[idx].password = keptPassword;
    } else {
        secrets_.writePassword(config.id, config.password);
    }
    settings_.setAccounts(accounts_);

    engine_.removeAccount(config.id);
    RegistrationInfo resetReg{config.id, RegState::Unregistered, 0, {}};
    registrations_.insert(config.id, resetReg);
    emit registrationChanged(resetReg);

    const bool ok = engine_.addAccount(accounts_[idx], error);
    if (ok) {
        applyOwnPresenceToAccount(config.id);
    }
    emit accountsChanged();
    return ok;
}

void AccountService::removeAccount(const QString &accountId) {
    const qsizetype idx = findAccountIndex(accountId);
    if (idx < 0) {
        return;
    }

    ++accountRevisions_[accountId];
    accounts_.removeAt(idx);
    accountRevisions_.remove(accountId);
    registrations_.remove(accountId);
    engine_.removeAccount(accountId);
    secrets_.deletePassword(accountId);
    settings_.setAccounts(accounts_);

    if (defaultAccountId_ == accountId) {
        defaultAccountId_.clear();
        normalizeDefaultAccountId();
        emit defaultAccountChanged(defaultAccountId_);
    }
    emit accountsChanged();
}

RegistrationInfo AccountService::registrationInfo(const QString &accountId) const {
    return registrations_.value(accountId,
                                RegistrationInfo{accountId, RegState::Unregistered, 0, {}});
}

void AccountService::handleRegistrationChanged(const RegistrationInfo &info) {
    registrations_.insert(info.accountId, info);
    if (info.state == RegState::Registered) {
        applyOwnPresenceToAccount(info.accountId);
    }
    emit registrationChanged(info);
}

PresenceState AccountService::ownPresence() const { return ownPresence_; }

bool AccountService::setOwnPresence(PresenceState state, const QString &note) {
    ownPresence_ = state;
    ownPresenceNote_ = note;
    bool ok = false;
    if (!defaultAccountId_.isEmpty()) {
        ok = engine_.setOwnPresence(defaultAccountId_, state, note);
    }
    emit ownPresenceChanged(state);
    return ok;
}

qsizetype AccountService::findAccountIndex(const QString &accountId) const {
    for (qsizetype i = 0; i < accounts_.size(); ++i) {
        if (accounts_[i].id == accountId) {
            return i;
        }
    }
    return -1;
}

void AccountService::normalizeDefaultAccountId() {
    if (!defaultAccountId_.isEmpty() && findAccountIndex(defaultAccountId_) >= 0) {
        return;
    }
    defaultAccountId_ = accounts_.isEmpty() ? QString{} : accounts_.first().id;
    settings_.setDefaultAccountId(defaultAccountId_);
}

void AccountService::applyOwnPresenceToAccount(const QString &accountId) {
    const auto acc = account(accountId);
    if (acc.has_value() && acc->publishPresence && accountId == defaultAccountId_) {
        (void)engine_.setOwnPresence(accountId, ownPresence_, ownPresenceNote_);
    }
}

} // namespace tincan
