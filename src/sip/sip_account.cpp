// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "sip/sip_account.h"

#include "core/sip_headers.h"

#include <utility>

namespace tincan::sip {

namespace {

RegState mapRegState(const pj::OnRegStateParam &prm) {
    if (prm.status == PJ_SUCCESS && (prm.code / 100 == 2)) {
        return prm.expiration > 0 ? RegState::Registered : RegState::Unregistered;
    }
    if (prm.code / 100 == 1) {
        return RegState::Registering;
    }
    return RegState::Failed;
}

} // namespace

SipAccount::SipAccount(AccountConfig config, pj::Endpoint &endpoint, EventMarshaller &marshaller,
                       RegChangedHandler onRegChanged, IncomingCallHandler onIncomingCall,
                       SipCall::Callbacks callCallbacks)
    : config_(std::move(config)), endpoint_(endpoint), marshaller_(marshaller),
      onRegChanged_(std::move(onRegChanged)), onIncomingCall_(std::move(onIncomingCall)),
      callCallbacks_(std::move(callCallbacks)) {}

SipAccount::~SipAccount() {
    shutdown();
}

void SipAccount::onRegStarted(pj::OnRegStartedParam &prm) {
    if (!prm.renew) {
        return;
    }
    RegistrationInfo info{config_.id, RegState::Registering, 100, QStringLiteral("Registering")};
    const RegChangedHandler handler = onRegChanged_;
    marshaller_.post([handler, info]() {
        if (handler) {
            handler(info);
        }
    });
}

void SipAccount::onRegState(pj::OnRegStateParam &prm) {
    RegistrationInfo info;
    info.accountId = config_.id;
    info.state = mapRegState(prm);
    info.statusCode = static_cast<int>(prm.code);
    info.reason = QString::fromStdString(prm.reason);

    const RegChangedHandler handler = onRegChanged_;
    marshaller_.post([handler, info]() {
        if (handler) {
            handler(info);
        }
    });
}

void SipAccount::onIncomingCall(pj::OnIncomingCallParam &prm) {
    try {
        auto call = std::make_unique<SipCall>(endpoint_, *this, config_.id, marshaller_,
                                              callCallbacks_, prm.callId);
        call->setIncomingHeaders(
            SipHeaders::parseIncomingHeaders(QString::fromStdString(prm.rdata.wholeMsg)));

        CallInfo incomingInfo = call->snapshot();
        incomingInfo.direction = CallDirection::Incoming;
        incomingInfo.state = CallState::Incoming;

        // Send 180 before handing ownership to the main thread: once posted,
        // the main thread may answer, reject, redirect or delete the call at any time.
        pj::CallOpParam ringPrm;
        ringPrm.statusCode = PJSIP_SC_RINGING;
        call->answer(ringPrm);

        auto holder = std::make_shared<std::unique_ptr<SipCall>>(std::move(call));
        const IncomingCallHandler handler = onIncomingCall_;
        marshaller_.post([handler, holder, incomingInfo]() {
            if (handler && *holder != nullptr) {
                handler(std::move(*holder), incomingInfo);
            }
        });
    } catch (const pj::Error &) {
    }
}

const AccountConfig &SipAccount::config() const {
    return config_;
}

} // namespace tincan::sip
