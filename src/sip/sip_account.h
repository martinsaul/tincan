// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include "core/account_config.h"
#include "core/call_info.h"
#include "core/message_info.h"
#include "core/presence_info.h"
#include "core/registration_info.h"
#include "sip/event_marshaller.h"
#include "sip/sip_call.h"

#include <pjsua2/account.hpp>
#include <pjsua2/endpoint.hpp>

#include <functional>
#include <memory>

namespace tincan::sip {

class SipAccount : public pj::Account {
public:
    struct AccountCallbacks {
        std::function<void(const RegistrationInfo &)> onRegChanged;
        std::function<void(std::unique_ptr<SipCall>, const CallInfo &)> onIncomingCall;
        std::function<void(const MessageInfo &)> onIncomingMessage;
        std::function<void(quint64, int, const QString &)> onMessageStatus;
    };

    SipAccount(AccountConfig config, pj::Endpoint &endpoint, EventMarshaller &marshaller,
               AccountCallbacks accountCallbacks, SipCall::Callbacks callCallbacks);
    ~SipAccount() override;

    void onRegStarted(pj::OnRegStartedParam &prm) override;
    void onRegState(pj::OnRegStateParam &prm) override;
    void onIncomingCall(pj::OnIncomingCallParam &prm) override;
    void onInstantMessage(pj::OnInstantMessageParam &prm) override;
    void onInstantMessageStatus(pj::OnInstantMessageStatusParam &prm) override;

    bool setOwnPresence(PresenceState state, const QString &note);
    bool sendMessage(const QString &targetUri, const QString &body, quint64 token);

    const AccountConfig &config() const;

private:
    AccountConfig config_;
    pj::Endpoint &endpoint_;
    EventMarshaller &marshaller_;
    AccountCallbacks accountCallbacks_;
    SipCall::Callbacks callCallbacks_;
};

} // namespace tincan::sip
