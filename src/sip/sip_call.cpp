// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "sip/sip_call.h"

#include "sip/call_info_mapper.h"

#include <cstddef>
#include <utility>

namespace tincan::sip {

SipCall::SipCall(pj::Endpoint &endpoint, pj::Account &account, QString accountId,
                 EventMarshaller &marshaller, StateChangedHandler onStateChanged, int callId)
    : pj::Call(account, callId), endpoint_(endpoint), accountId_(std::move(accountId)),
      marshaller_(marshaller), onStateChanged_(std::move(onStateChanged)) {}

void SipCall::onCallState(pj::OnCallStateParam &prm) {
    PJ_UNUSED_ARG(prm);
    try {
        const pj::CallInfo pjInfo = getInfo();
        if (pjInfo.state == PJSIP_INV_STATE_CONFIRMED) {
            qint64 expected = 0;
            const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
            connectedAtMs_.compare_exchange_strong(expected, nowMs);
        }

        const CallInfo info = CallInfoMapper::map(pjInfo, accountId_, connectedTimestamp());
        const StateChangedHandler handler = onStateChanged_;
        const SipCall *self = this;
        marshaller_.post([handler, self, info]() {
            if (handler) {
                handler(self, info);
            }
        });
    } catch (const pj::Error &) {
    }
}

void SipCall::onCallMediaState(pj::OnCallMediaStateParam &prm) {
    PJ_UNUSED_ARG(prm);
    try {
        const pj::CallInfo pjInfo = getInfo();
        pj::AudDevManager &audMgr = endpoint_.audDevManager();
        for (std::size_t i = 0; i < pjInfo.media.size(); ++i) {
            const pj::CallMediaInfo &med = pjInfo.media[i];
            if (med.type == PJMEDIA_TYPE_AUDIO && med.status == PJSUA_CALL_MEDIA_ACTIVE) {
                pj::AudioMedia callAudio = getAudioMedia(static_cast<int>(i));
                callAudio.startTransmit(audMgr.getPlaybackDevMedia());
                audMgr.getCaptureDevMedia().startTransmit(callAudio);
            }
        }
    } catch (const pj::Error &) {
    }
}

CallInfo SipCall::snapshot() const {
    const pj::CallInfo pjInfo = getInfo();
    return CallInfoMapper::map(pjInfo, accountId_, connectedTimestamp());
}

QDateTime SipCall::connectedTimestamp() const {
    const qint64 ms = connectedAtMs_.load();
    return ms > 0 ? QDateTime::fromMSecsSinceEpoch(ms) : QDateTime{};
}

} // namespace tincan::sip
