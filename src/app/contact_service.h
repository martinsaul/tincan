// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include "app/account_service.h"
#include "app/contact_csv.h"
#include "app/contact_repository.h"
#include "core/contact.h"
#include "core/presence_info.h"
#include "core/sip_engine.h"
#include "core/sip_events.h"

#include <QHash>
#include <QList>
#include <QObject>
#include <QString>

#include <optional>

namespace tincan {

class ContactService : public QObject, public ISipEvents {
    Q_OBJECT

public:
    ContactService(ISipEngine &engine, AccountService &accountService,
                   ContactRepository &repository, QObject *parent = nullptr);

    QList<Contact> contacts() const;
    QList<Contact> search(const QString &queryText) const;
    std::optional<Contact> contact(qint64 id) const;

    qint64 addContact(const Contact &contact);
    bool updateContact(const Contact &contact);
    bool removeContact(qint64 id);

    ContactCsv::ImportResult importCsv(const QString &csvText);
    QString exportCsv() const;

    PresenceInfo presenceForContact(const Contact &contact) const;
    PresenceInfo presenceForUri(const QString &uri) const;
    QString resolveDisplayName(const QString &numberOrUri) const;

    void onPresenceChanged(const PresenceInfo &info) override;

signals:
    void contactsChanged();
    void presenceChanged(const tincan::PresenceInfo &info);

private:
    struct ActiveSubscription {
        QString accountId;
        QString uri;
    };

    void reloadCache();
    void syncPresenceSubscriptions();
    QString effectiveAccountId(const Contact &contact) const;
    QString resolvedContactUri(const Contact &contact) const;
    bool isAccountReadyForPresence(const QString &accountId) const;
    static QString canonicalUriKey(const QString &uri);

    ISipEngine &engine_;
    AccountService &accountService_;
    ContactRepository &repository_;
    QList<Contact> cachedContacts_;
    QHash<qint64, ActiveSubscription> activeSubscriptions_;
    QHash<QString, PresenceInfo> livePresence_;
};

} // namespace tincan
