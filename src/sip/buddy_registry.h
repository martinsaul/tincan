// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include "core/sip_events.h"
#include "sip/event_marshaller.h"
#include "sip/sip_buddy.h"

#include <QString>

#include <map>
#include <memory>

namespace tincan::sip {

class SipAccount;

class BuddyRegistry {
public:
    BuddyRegistry(ISipEvents &events, EventMarshaller &marshaller);
    ~BuddyRegistry() = default;

    BuddyRegistry(const BuddyRegistry &) = delete;
    BuddyRegistry &operator=(const BuddyRegistry &) = delete;

    bool subscribe(SipAccount &account, const QString &accountId, const QString &targetUri);
    void unsubscribe(const QString &uri);
    void removeForAccount(const QString &accountId);
    void clear();

private:
    static QString normalizeKey(const QString &uri);

    ISipEvents &events_;
    EventMarshaller &marshaller_;
    std::map<QString, std::unique_ptr<SipBuddy>> buddies_;
};

} // namespace tincan::sip
