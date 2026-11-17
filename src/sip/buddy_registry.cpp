// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "sip/buddy_registry.h"

#include "core/sip_uri.h"
#include "sip/sip_account.h"

namespace tincan::sip {

BuddyRegistry::BuddyRegistry(ISipEvents &events, EventMarshaller &marshaller)
    : events_(events), marshaller_(marshaller) {}

QString BuddyRegistry::normalizeKey(const QString &uri) {
    const QString bare = SipUri::bareSipUri(uri);
    return (bare.isEmpty() ? uri.trimmed() : bare).toLower();
}

bool BuddyRegistry::subscribe(SipAccount &account, const QString &accountId,
                              const QString &targetUri) {
    const QString key = normalizeKey(targetUri);
    if (key.isEmpty()) {
        return false;
    }

    const auto existing = buddies_.find(key);
    if (existing != buddies_.end()) {
        if (existing->second->accountId() == accountId && existing->second->isValid()) {
            try {
                existing->second->updatePresence();
            } catch (const pj::Error &) {
            }
            return true;
        }
        buddies_.erase(existing);
    }

    try {
        auto buddy = std::make_unique<SipBuddy>(
            accountId, targetUri, marshaller_, [this, key](const PresenceInfo &info) {
                if (buddies_.find(key) != buddies_.end()) {
                    events_.onPresenceChanged(info);
                }
            });
        buddy->startSubscription(account);
        buddies_.emplace(key, std::move(buddy));
        return true;
    } catch (const pj::Error &) {
        return false;
    }
}

void BuddyRegistry::unsubscribe(const QString &uri) {
    const QString key = normalizeKey(uri);
    const auto it = buddies_.find(key);
    if (it != buddies_.end()) {
        buddies_.erase(it);
        return;
    }

    const QString trimmed = uri.trimmed();
    for (auto scan = buddies_.begin(); scan != buddies_.end(); ++scan) {
        if (scan->second->uri().compare(trimmed, Qt::CaseInsensitive) == 0 ||
            SipUri::extractUserPart(scan->second->uri()).compare(trimmed, Qt::CaseInsensitive) ==
                0) {
            buddies_.erase(scan);
            return;
        }
    }
}

void BuddyRegistry::removeForAccount(const QString &accountId) {
    for (auto it = buddies_.begin(); it != buddies_.end();) {
        if (it->second->accountId() == accountId) {
            it = buddies_.erase(it);
        } else {
            ++it;
        }
    }
}

void BuddyRegistry::clear() {
    buddies_.clear();
}

} // namespace tincan::sip
