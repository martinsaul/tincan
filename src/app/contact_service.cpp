// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "app/contact_service.h"

#include "core/sip_uri.h"

namespace tincan {

ContactService::ContactService(ISipEngine &engine, AccountService &accountService,
                               ContactRepository &repository, QObject *parent)
    : QObject(parent), engine_(engine), accountService_(accountService), repository_(repository) {
    reloadCache();

    connect(&accountService_, &AccountService::accountsChanged, this,
            &ContactService::syncPresenceSubscriptions);
    connect(&accountService_, &AccountService::defaultAccountChanged, this,
            [this](const QString &) { syncPresenceSubscriptions(); });
    connect(&accountService_, &AccountService::registrationChanged, this,
            [this](const RegistrationInfo &) { syncPresenceSubscriptions(); });

    syncPresenceSubscriptions();
}

QList<Contact> ContactService::contacts() const { return cachedContacts_; }

QList<Contact> ContactService::search(const QString &queryText) const {
    return repository_.search(queryText);
}

std::optional<Contact> ContactService::contact(qint64 id) const {
    return repository_.findById(id);
}

qint64 ContactService::addContact(const Contact &contact) {
    if (contact.name.trimmed().isEmpty() || contact.number.trimmed().isEmpty()) {
        return -1;
    }
    Contact cleaned = contact;
    cleaned.name = cleaned.name.trimmed();
    cleaned.number = cleaned.number.trimmed();
    cleaned.accountId = cleaned.accountId.trimmed();

    const qint64 id = repository_.insert(cleaned);
    if (id <= 0) {
        return -1;
    }
    reloadCache();
    syncPresenceSubscriptions();
    emit contactsChanged();
    return id;
}

bool ContactService::updateContact(const Contact &contact) {
    if (contact.id <= 0 || contact.name.trimmed().isEmpty() || contact.number.trimmed().isEmpty()) {
        return false;
    }
    Contact cleaned = contact;
    cleaned.name = cleaned.name.trimmed();
    cleaned.number = cleaned.number.trimmed();
    cleaned.accountId = cleaned.accountId.trimmed();

    if (!repository_.update(cleaned)) {
        return false;
    }
    reloadCache();
    syncPresenceSubscriptions();
    emit contactsChanged();
    return true;
}

bool ContactService::removeContact(qint64 id) {
    if (!repository_.remove(id)) {
        return false;
    }
    reloadCache();
    syncPresenceSubscriptions();
    emit contactsChanged();
    return true;
}

ContactCsv::ImportResult ContactService::importCsv(const QString &csvText) {
    ContactCsv::ImportResult parsed = ContactCsv::parseContacts(csvText, cachedContacts_);
    if (parsed.contacts.isEmpty()) {
        return parsed;
    }
    QList<Contact> inserted;
    inserted.reserve(parsed.contacts.size());
    for (Contact c : parsed.contacts) {
        const qint64 id = repository_.insert(c);
        if (id > 0) {
            c.id = id;
            inserted.append(c);
        }
    }
    parsed.contacts = inserted;
    parsed.imported = static_cast<int>(inserted.size());
    reloadCache();
    syncPresenceSubscriptions();
    emit contactsChanged();
    return parsed;
}

QString ContactService::exportCsv() const {
    return ContactCsv::exportContacts(cachedContacts_);
}

PresenceInfo ContactService::presenceForContact(const Contact &contact) const {
    const QString uri = resolvedContactUri(contact);
    if (!uri.isEmpty()) {
        const auto it = livePresence_.find(canonicalUriKey(uri));
        if (it != livePresence_.end()) {
            return *it;
        }
    }
    return PresenceInfo{uri, PresenceState::Unknown, {}};
}

PresenceInfo ContactService::presenceForUri(const QString &uri) const {
    const auto it = livePresence_.find(canonicalUriKey(uri));
    if (it != livePresence_.end()) {
        return *it;
    }
    return PresenceInfo{uri, PresenceState::Unknown, {}};
}

QString ContactService::resolveDisplayName(const QString &numberOrUri) const {
    const QString trimmed = numberOrUri.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }

    const QString targetBare = canonicalUriKey(trimmed);
    if (!targetBare.isEmpty()) {
        for (const Contact &c : cachedContacts_) {
            const QString contactUri = canonicalUriKey(resolvedContactUri(c));
            if (!contactUri.isEmpty() && contactUri == targetBare) {
                return c.name;
            }
            if (canonicalUriKey(c.number) == targetBare) {
                return c.name;
            }
        }
    }

    const QString userPart = SipUri::extractUserPart(trimmed);
    const QString targetDial =
        SipUri::normalizeDialString(userPart.isEmpty() ? trimmed : userPart).toLower();
    if (targetDial.isEmpty()) {
        return {};
    }

    for (const Contact &c : cachedContacts_) {
        const QString cUser = SipUri::extractUserPart(c.number);
        const QString cDial =
            SipUri::normalizeDialString(cUser.isEmpty() ? c.number : cUser).toLower();
        if (!cDial.isEmpty() && cDial == targetDial) {
            return c.name;
        }
    }
    return {};
}

void ContactService::onPresenceChanged(const PresenceInfo &info) {
    const QString key = canonicalUriKey(info.uri);
    if (!key.isEmpty()) {
        livePresence_.insert(key, info);
    }
    emit presenceChanged(info);
}

void ContactService::reloadCache() {
    cachedContacts_ = repository_.listAll();
}

void ContactService::syncPresenceSubscriptions() {
    QHash<qint64, ActiveSubscription> desired;
    for (const Contact &c : cachedContacts_) {
        if (!c.presence) {
            continue;
        }
        const QString accId = effectiveAccountId(c);
        if (!isAccountReadyForPresence(accId)) {
            continue;
        }
        const QString uri = resolvedContactUri(c);
        if (!uri.isEmpty()) {
            desired.insert(c.id, ActiveSubscription{accId, uri});
        }
    }

    for (auto it = activeSubscriptions_.begin(); it != activeSubscriptions_.end();) {
        const auto want = desired.find(it.key());
        if (want == desired.end() || want->accountId != it->accountId || want->uri != it->uri) {
            engine_.unsubscribePresence(it->uri);
            livePresence_.remove(canonicalUriKey(it->uri));
            it = activeSubscriptions_.erase(it);
        } else {
            ++it;
        }
    }

    for (auto it = desired.cbegin(); it != desired.cend(); ++it) {
        if (!activeSubscriptions_.contains(it.key())) {
            if (engine_.subscribePresence(it->accountId, it->uri)) {
                activeSubscriptions_.insert(it.key(), *it);
            }
        }
    }
}

QString ContactService::effectiveAccountId(const Contact &contact) const {
    return contact.accountId.trimmed().isEmpty() ? accountService_.defaultAccountId()
                                                 : contact.accountId.trimmed();
}

QString ContactService::resolvedContactUri(const Contact &contact) const {
    const QString accId = effectiveAccountId(contact);
    if (const auto acc = accountService_.account(accId); acc.has_value()) {
        return SipUri::toSipUri(contact.number, *acc);
    }
    return SipUri::toSipUri(contact.number, AccountConfig{});
}

bool ContactService::isAccountReadyForPresence(const QString &accountId) const {
    if (accountId.isEmpty()) {
        return false;
    }
    const auto acc = accountService_.account(accountId);
    if (!acc.has_value()) {
        return false;
    }
    if (!acc->registerAccount) {
        return true;
    }
    return accountService_.registrationInfo(accountId).state == RegState::Registered;
}

QString ContactService::canonicalUriKey(const QString &uri) {
    return SipUri::bareSipUri(uri).toLower();
}

} // namespace tincan
