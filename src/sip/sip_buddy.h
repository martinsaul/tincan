// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include "core/presence_info.h"
#include "sip/event_marshaller.h"

#include <pjsua2/account.hpp>
#include <pjsua2/presence.hpp>

#include <QString>

#include <functional>

namespace tincan::sip {

class SipBuddy : public pj::Buddy {
public:
    using PresenceHandler = std::function<void(const PresenceInfo &)>;

    SipBuddy(QString accountId, QString uri, EventMarshaller &marshaller,
             PresenceHandler onPresenceChanged);
    ~SipBuddy() override;

    SipBuddy(const SipBuddy &) = delete;
    SipBuddy &operator=(const SipBuddy &) = delete;

    void startSubscription(pj::Account &account);
    void stopSubscription();

    const QString &accountId() const;
    const QString &uri() const;

    void onBuddyState() override;
    void onBuddyDlgEventState() override;

private:
    QString accountId_;
    QString uri_;
    EventMarshaller &marshaller_;
    PresenceHandler onPresenceChanged_;
};

} // namespace tincan::sip
