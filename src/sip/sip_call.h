// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include "core/call_info.h"
#include "sip/event_marshaller.h"

#include <QDateTime>
#include <QString>

#include <pjsua2/call.hpp>
#include <pjsua2/endpoint.hpp>

#include <atomic>
#include <functional>

namespace tincan::sip {

class SipCall : public pj::Call {
public:
    // The SipCall pointer is an identity token only; it must never be dereferenced
    // by the handler (the object may already be gone).
    using StateChangedHandler = std::function<void(const SipCall *, const CallInfo &)>;

    SipCall(pj::Endpoint &endpoint, pj::Account &account, QString accountId,
            EventMarshaller &marshaller, StateChangedHandler onStateChanged,
            int callId = PJSUA_INVALID_ID);
    ~SipCall() override = default;

    void onCallState(pj::OnCallStateParam &prm) override;
    void onCallMediaState(pj::OnCallMediaStateParam &prm) override;

    CallInfo snapshot() const;

private:
    QDateTime connectedTimestamp() const;

    pj::Endpoint &endpoint_;
    QString accountId_;
    EventMarshaller &marshaller_;
    StateChangedHandler onStateChanged_;
    std::atomic<qint64> connectedAtMs_{0};
};

} // namespace tincan::sip
