// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include "app/secret_store.h"
#include "app/settings_store.h"
#include "core/account_config.h"
#include "core/presence_info.h"
#include "core/registration_info.h"
#include "core/sip_engine.h"

#include <QHash>
#include <QList>
#include <QObject>
#include <QString>

#include <optional>

namespace tincan {

class AccountService : public QObject {
    Q_OBJECT

public:
    AccountService(ISipEngine &engine, SettingsStore &settings, ISecretStore &secrets,
                   QObject *parent = nullptr);

    void load();

    QList<AccountConfig> accounts() const;
    std::optional<AccountConfig> account(const QString &accountId) const;
    std::optional<AccountConfig> defaultAccount() const;

    QString defaultAccountId() const;
    void setDefaultAccountId(const QString &accountId);

    bool addAccount(AccountConfig config, QString *error = nullptr);
    bool updateAccount(const AccountConfig &config, QString *error = nullptr);
    void removeAccount(const QString &accountId);

    RegistrationInfo registrationInfo(const QString &accountId) const;
    void handleRegistrationChanged(const RegistrationInfo &info);

    PresenceState ownPresence() const;
    bool setOwnPresence(PresenceState state, const QString &note = {});

signals:
    void accountsChanged();
    void defaultAccountChanged(const QString &accountId);
    void registrationChanged(const tincan::RegistrationInfo &info);
    void ownPresenceChanged(tincan::PresenceState state);

private:
    qsizetype findAccountIndex(const QString &accountId) const;
    void normalizeDefaultAccountId();
    void applyOwnPresenceToAccount(const QString &accountId);

    ISipEngine &engine_;
    SettingsStore &settings_;
    ISecretStore &secrets_;
    QList<AccountConfig> accounts_;
    QString defaultAccountId_;
    QHash<QString, RegistrationInfo> registrations_;
    QHash<QString, quint64> accountRevisions_;
    PresenceState ownPresence_ = PresenceState::Online;
    QString ownPresenceNote_;
};

} // namespace tincan
