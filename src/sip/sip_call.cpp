// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "sip/sip_call.h"

#include "sip/call_info_mapper.h"
#include "sip/call_slot.h"

#include <algorithm>
#include <cstddef>
#include <utility>

namespace tincan::sip {

namespace {

bool isValidInbandDigit(char ch) {
    return (ch >= '0' && ch <= '9') || ch == '*' || ch == '#' || (ch >= 'A' && ch <= 'D') ||
           (ch >= 'a' && ch <= 'd');
}

} // namespace

SipCall::SipCall(pj::Endpoint &endpoint, pj::Account &account, QString accountId,
                 EventMarshaller &marshaller, Callbacks callbacks, int callId)
    : pj::Call(account, callId), endpoint_(endpoint), account_(account),
      accountId_(std::move(accountId)), marshaller_(marshaller), callbacks_(std::move(callbacks)) {}

SipCall::~SipCall() {
    stopInbandDtmf();
    if (disconnected_.load()) {
        // The id may already belong to a new call; keep ~Call away from it.
        forgetCallSlot(*this);
    }
}

bool SipCall::isDisconnected() const {
    return disconnected_.load();
}

bool SipCall::isMuted() const {
    return muted_.load();
}

void SipCall::onCreateMediaTransport(pj::OnCreateMediaTransportParam &prm) {
    PJ_UNUSED_ARG(prm);
    adoptSelfIfNeeded();
}

void SipCall::onCallState(pj::OnCallStateParam &prm) {
    PJ_UNUSED_ARG(prm);
    adoptSelfIfNeeded();
    try {
        const pj::CallInfo pjInfo = getInfo();
        if (pjInfo.state == PJSIP_INV_STATE_CONFIRMED) {
            qint64 expected = 0;
            const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
            connectedAtMs_.compare_exchange_strong(expected, nowMs);
        }
        if (pjInfo.state == PJSIP_INV_STATE_DISCONNECTED) {
            disconnected_.store(true);
        }

        const CallInfo info = snapshot();
        const auto handler = callbacks_.onStateChanged;
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
    adoptSelfIfNeeded();
    try {
        const pj::CallInfo pjInfo = getInfo();
        pj::AudDevManager &audMgr = endpoint_.audDevManager();
        for (std::size_t i = 0; i < pjInfo.media.size(); ++i) {
            const pj::CallMediaInfo &med = pjInfo.media[i];
            if (med.type != PJMEDIA_TYPE_AUDIO || med.audioConfSlot == PJSUA_INVALID_ID) {
                continue;
            }
            if (med.status == PJSUA_CALL_MEDIA_ACTIVE ||
                med.status == PJSUA_CALL_MEDIA_REMOTE_HOLD) {
                pj::AudioMedia callAudio = getAudioMedia(static_cast<int>(i));
                applyMediaLevels(callAudio);
                callAudio.startTransmit(audMgr.getPlaybackDevMedia());
                // The microphone is connected only on the main thread
                // (applyCaptureRouting), so it can never race with setMuted.
            }
        }

        const CallInfo info = snapshot();
        const auto handler = callbacks_.onMediaChanged;
        const SipCall *self = this;
        marshaller_.post([handler, self, info]() {
            if (handler) {
                handler(self, info);
            }
        });
    } catch (const pj::Error &) {
    }
}

void SipCall::onCallTransferStatus(pj::OnCallTransferStatusParam &prm) {
    const int status = static_cast<int>(prm.statusCode);
    transferStatus_.store(status);
    if (status >= 200 && status < 300) {
        prm.cont = false;
    }

    try {
        const CallInfo info = snapshot();
        const auto handler = callbacks_.onTransferStatus;
        const SipCall *self = this;
        marshaller_.post([handler, self, info]() {
            if (handler) {
                handler(self, info);
            }
        });
    } catch (const pj::Error &) {
    }
}

void SipCall::onCallTransferRequest(pj::OnCallTransferRequestParam &prm) {
    prm.opt.audioCount = 1;
    prm.opt.videoCount = 0;
    prm.opt.textCount = 0;
    prm.newCall = spawnPendingChild();
}

void SipCall::onCallReplaceRequest(pj::OnCallReplaceRequestParam &prm) {
    prm.opt.audioCount = 1;
    prm.opt.videoCount = 0;
    prm.opt.textCount = 0;
    prm.newCall = spawnPendingChild();
}

void SipCall::onCallReplaced(pj::OnCallReplacedParam &prm) {
    if (prm.newCall != nullptr) {
        return;
    }
    try {
        auto child = std::make_unique<SipCall>(endpoint_, account_, accountId_, marshaller_,
                                               callbacks_, prm.newCallId);
        prm.newCall = child.get();
        const CallInfo info = child->snapshot();
        auto claimed = std::make_shared<std::unique_ptr<SipCall>>(std::move(child));
        const auto handler = callbacks_.onChildCreated;
        marshaller_.post([handler, claimed, info]() {
            if (handler && *claimed != nullptr) {
                handler(std::move(*claimed), info);
            }
        });
    } catch (const pj::Error &) {
    }
}

bool SipCall::setHoldState(bool hold) {
    if (disconnected_.load()) {
        return false;
    }
    try {
        if (hold) {
            pj::CallOpParam prm;
            setHold(prm);
        } else {
            pj::CallOpParam prm(true);
            prm.opt.audioCount = 1;
            prm.opt.videoCount = 0;
            prm.opt.textCount = 0;
            prm.opt.flag |= PJSUA_CALL_UNHOLD;
            reinvite(prm);
        }
        return true;
    } catch (const pj::Error &) {
        return false;
    }
}

bool SipCall::setMuted(bool mute) {
    muted_.store(mute);
    applyCaptureRouting();
    return true;
}

bool SipCall::isCaptureConnected() const {
    pj::AudioMedia callAudio;
    if (!tryGetAudioMedia(&callAudio)) {
        return false;
    }
    try {
        const pj::ConfPortInfo capture =
            endpoint_.audDevManager().getCaptureDevMedia().getPortInfo();
        for (const int listener : capture.listeners) {
            if (listener == callAudio.getPortId()) {
                return true;
            }
        }
    } catch (const pj::Error &) {
    }
    return false;
}

void SipCall::applyCaptureRouting() {
    if (disconnected_.load()) {
        return;
    }
    pj::CallInfo pjInfo;
    try {
        pjInfo = getInfo();
    } catch (const pj::Error &) {
        return;
    }
    pj::AudDevManager &audMgr = endpoint_.audDevManager();
    for (std::size_t i = 0; i < pjInfo.media.size(); ++i) {
        const pj::CallMediaInfo &med = pjInfo.media[i];
        if (med.type != PJMEDIA_TYPE_AUDIO || med.audioConfSlot == PJSUA_INVALID_ID) {
            continue;
        }
        try {
            pj::AudioMedia callAudio = getAudioMedia(static_cast<int>(i));
            if (med.status == PJSUA_CALL_MEDIA_ACTIVE && !muted_.load()) {
                audMgr.getCaptureDevMedia().startTransmit(callAudio);
            } else {
                audMgr.getCaptureDevMedia().stopTransmit(callAudio);
            }
        } catch (const pj::Error &) {
        }
    }
}

bool SipCall::sendDtmfDigits(const QString &digits, DtmfMethod method) {
    if (digits.isEmpty() || disconnected_.load()) {
        return false;
    }
    if (method == DtmfMethod::InBand) {
        return sendInbandDtmf(digits);
    }
    try {
        pj::CallSendDtmfParam prm;
        prm.method =
            method == DtmfMethod::SipInfo ? PJSUA_DTMF_METHOD_SIP_INFO : PJSUA_DTMF_METHOD_RFC2833;
        prm.digits = digits.toStdString();
        sendDtmf(prm);
        return true;
    } catch (const pj::Error &) {
        return false;
    }
}

bool SipCall::sendInbandDtmf(const QString &digits) {
    pj::AudioMedia callAudio;
    if (!tryGetAudioMedia(&callAudio)) {
        return false;
    }

    pj::ToneDigitVector toneDigits;
    const std::string ascii = digits.toStdString();
    for (const char ch : ascii) {
        if (!isValidInbandDigit(ch)) {
            continue;
        }
        pj::ToneDigit td;
        td.digit = ch;
        td.on_msec = 100;
        td.off_msec = 50;
        td.volume = 0;
        toneDigits.push_back(td);
    }
    if (toneDigits.empty()) {
        return false;
    }

    try {
        if (!inbandDtmfGen_) {
            inbandDtmfGen_ = std::make_unique<pj::ToneGenerator>();
            inbandDtmfGen_->createToneGenerator();
        }
        inbandDtmfGen_->startTransmit(callAudio);
        inbandDtmfGen_->playDigits(toneDigits, false);
        return true;
    } catch (const pj::Error &) {
        return false;
    }
}

bool SipCall::setVolume(float rxLevel, float txLevel) {
    const float clampedRx = std::clamp(rxLevel, 0.0f, 2.0f);
    const float clampedTx = std::clamp(txLevel, 0.0f, 2.0f);
    rxLevel_.store(clampedRx);
    txLevel_.store(clampedTx);

    pj::AudioMedia callAudio;
    if (tryGetAudioMedia(&callAudio)) {
        applyMediaLevels(callAudio);
    }
    return true;
}

void SipCall::stopInbandDtmf() {
    if (!inbandDtmfGen_) {
        return;
    }
    try {
        inbandDtmfGen_->stop();
    } catch (const pj::Error &) {
    }
    inbandDtmfGen_.reset();
}

bool SipCall::tryGetAudioMedia(pj::AudioMedia *outMedia) const {
    if (outMedia == nullptr || disconnected_.load()) {
        return false;
    }
    try {
        *outMedia = getAudioMedia(-1);
        return true;
    } catch (const pj::Error &) {
        return false;
    }
}

void SipCall::setIncomingHeaders(QMap<QString, QString> headers) {
    incomingHeaders_ = std::move(headers);
}

const QString &SipCall::accountId() const {
    return accountId_;
}

CallInfo SipCall::snapshot() const {
    const pj::CallInfo pjInfo = getInfo();
    CallInfo info = CallInfoMapper::map(pjInfo, accountId_, connectedTimestamp());
    info.muted = muted_.load();
    info.micConnected = isCaptureConnected();
    info.transferStatus = transferStatus_.load();
    info.rxLevel = rxLevel_.load();
    info.txLevel = txLevel_.load();
    info.incomingHeaders = incomingHeaders_;
    return info;
}

QDateTime SipCall::connectedTimestamp() const {
    const qint64 ms = connectedAtMs_.load();
    return ms > 0 ? QDateTime::fromMSecsSinceEpoch(ms) : QDateTime{};
}

SipCall *SipCall::spawnPendingChild() {
    if (pendingChild_ && *pendingChild_ != nullptr) {
        auto discarded = std::move(pendingChild_);
        marshaller_.post([discarded]() {});
    }
    auto child = std::make_unique<SipCall>(endpoint_, account_, accountId_, marshaller_, callbacks_,
                                           PJSUA_INVALID_ID);
    pendingChild_ = std::make_shared<std::unique_ptr<SipCall>>(std::move(child));
    SipCall *raw = pendingChild_->get();
    raw->selfHolder_ = pendingChild_;
    return raw;
}

void SipCall::adoptSelfIfNeeded() {
    if (getId() == PJSUA_INVALID_ID) {
        return;
    }
    const auto holder = selfHolder_.lock();
    if (!holder || *holder == nullptr) {
        return;
    }
    selfHolder_.reset();
    auto claimed = std::make_shared<std::unique_ptr<SipCall>>(std::move(*holder));
    try {
        const CallInfo info = snapshot();
        const auto handler = callbacks_.onChildCreated;
        marshaller_.post([handler, claimed, info]() {
            if (handler && *claimed != nullptr) {
                handler(std::move(*claimed), info);
            }
        });
    } catch (const pj::Error &) {
        marshaller_.post([claimed]() {});
    }
}

void SipCall::applyMediaLevels(pj::AudioMedia &callAudio) const {
    // In pjsua2, AudioMedia::adjustTxLevel adjusts signal FROM the call port INTO the
    // bridge (speaker / received audio), while adjustRxLevel adjusts signal FROM the
    // bridge INTO the call port (microphone / transmitted audio).
    try {
        callAudio.adjustTxLevel(rxLevel_.load());
        callAudio.adjustRxLevel(txLevel_.load());
    } catch (const pj::Error &) {
    }
}

} // namespace tincan::sip
