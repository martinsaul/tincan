// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include "core/account_config.h"
#include "core/call_info.h"
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
    using RegChangedHandler = std::function<void(const RegistrationInfo &)>;
    using IncomingCallHandler =
        std::function<void(std::unique_ptr<SipCall>, const CallInfo &)>;

    SipAccount(AccountConfig config, pj::Endpoint &endpoint, EventMarshaller &marshaller,
               RegChangedHandler onRegChanged, IncomingCallHandler onIncomingCall,
               SipCall::StateChangedHandler onCallStateChanged);
    ~SipAccount() override;

    void onRegStarted(pj::OnRegStartedParam &prm) override;
    void onRegState(pj::OnRegStateParam &prm) override;
    void onIncomingCall(pj::OnIncomingCallParam &prm) override;

    const AccountConfig &config() const;

private:
    AccountConfig config_;
    pj::Endpoint &endpoint_;
    EventMarshaller &marshaller_;
    RegChangedHandler onRegChanged_;
    IncomingCallHandler onIncomingCall_;
    SipCall::StateChangedHandler onCallStateChanged_;
};

} // namespace tincan::sip
