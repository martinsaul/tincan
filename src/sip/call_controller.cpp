// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "sip/call_controller.h"

#include "sip/call_recorder.h"
#include "sip/call_registry.h"
#include "sip/conference_bridge.h"
#include "sip/event_marshaller.h"
#include "sip/sip_account.h"
#include "sip/sip_endpoint.h"

#include <utility>

namespace tincan::sip {

namespace {

pj::CallOpParam makeAudioCallOpParam() {
    pj::CallOpParam prm(true);
    prm.opt.audioCount = 1;
    prm.opt.videoCount = 0;
    prm.opt.textCount = 0;
    return prm;
}

QString wrapAngleBrackets(const QString &uri) {
    const QString trimmed = uri.trimmed();
    if (trimmed.startsWith(u'<') && trimmed.endsWith(u'>')) {
        return trimmed;
    }
    return QStringLiteral("<%1>").arg(trimmed);
}

} // namespace

CallController::CallController(ISipEvents &events, SipEndpoint &endpoint,
                               EventMarshaller &marshaller, UriResolver uriResolver)
    : events_(events), endpoint_(endpoint), marshaller_(marshaller),
      uriResolver_(std::move(uriResolver)), recorder_(std::make_unique<CallRecorder>(endpoint)),
      conference_(std::make_unique<ConferenceBridge>()), calls_(std::make_unique<CallRegistry>()) {}

CallController::~CallController() = default;

SipCall::Callbacks CallController::makeCallCallbacks() {
    SipCall::Callbacks cbs;
    cbs.onStateChanged = [this](const SipCall *call, const CallInfo &info) {
        handleCallStateOnMainThread(call, info);
    };
    cbs.onMediaChanged = [this](const SipCall *call, const CallInfo &info) {
        handleCallMediaOnMainThread(call, info);
    };
    cbs.onTransferStatus = [this](const SipCall *call, const CallInfo &info) {
        handleTransferStatusOnMainThread(call, info);
    };
    cbs.onChildCreated = [this](std::unique_ptr<SipCall> call, const CallInfo &info) {
        handleChildCallOnMainThread(std::move(call), info);
    };
    return cbs;
}

void CallController::adoptIncomingCall(std::unique_ptr<SipCall> call, const CallInfo &info) {
    calls_->insert(info.callId, std::move(call));
    events_.onIncomingCall(enrichCallInfo(info));
}

int CallController::makeCall(SipAccount &account, const QString &accountId,
                             const QString &targetUri, QString *error) {
    try {
        auto call = std::make_unique<SipCall>(endpoint_.pjEndpoint(), account, accountId,
                                              marshaller_, makeCallCallbacks());
        call->makeCall(targetUri.toStdString(), makeAudioCallOpParam());
        const int callId = call->getId();
        calls_->insert(callId, std::move(call));
        return callId;
    } catch (const pj::Error &err) {
        if (error != nullptr) {
            *error = QString::fromStdString(err.info());
        }
        return -1;
    }
}

bool CallController::answer(int callId) {
    SipCall *call = calls_->find(callId);
    if (call == nullptr) {
        return false;
    }
    try {
        pj::CallOpParam prm = makeAudioCallOpParam();
        prm.statusCode = PJSIP_SC_OK;
        call->answer(prm);
        return true;
    } catch (const pj::Error &) {
        return false;
    }
}

bool CallController::reject(int callId, int statusCode) {
    if (statusCode < 300 || statusCode > 699) {
        return false;
    }
    SipCall *call = calls_->find(callId);
    if (call == nullptr) {
        return false;
    }
    try {
        pj::CallOpParam prm;
        prm.statusCode = static_cast<pjsip_status_code>(statusCode);
        call->hangup(prm);
        return true;
    } catch (const pj::Error &) {
        return false;
    }
}

bool CallController::redirect(int callId, const QString &uri) {
    SipCall *call = calls_->find(callId);
    if (call == nullptr) {
        return false;
    }
    const QString targetUri = uriResolver_ ? uriResolver_(call->accountId(), uri) : uri;
    if (targetUri.isEmpty()) {
        return false;
    }
    try {
        pj::CallOpParam prm;
        prm.statusCode = PJSIP_SC_MOVED_TEMPORARILY;
        pj::SipHeader contactHdr;
        contactHdr.hName = "Contact";
        contactHdr.hValue = wrapAngleBrackets(targetUri).toStdString();
        prm.txOption.headers.push_back(contactHdr);
        call->answer(prm);
        return true;
    } catch (const pj::Error &) {
        return false;
    }
}

bool CallController::hangup(int callId) {
    SipCall *call = calls_->find(callId);
    if (call == nullptr) {
        return false;
    }
    call->stopInbandDtmf();
    try {
        pj::CallOpParam prm;
        call->hangup(prm);
        return true;
    } catch (const pj::Error &) {
        return false;
    }
}

bool CallController::setHold(int callId, bool hold) {
    SipCall *call = calls_->find(callId);
    return call != nullptr && call->setHoldState(hold);
}

bool CallController::setMute(int callId, bool mute) {
    SipCall *call = calls_->find(callId);
    if (call == nullptr || !call->setMuted(mute)) {
        return false;
    }
    notifySnapshotChanged(callId);
    return true;
}

bool CallController::sendDtmf(int callId, const QString &digits, DtmfMethod method) {
    SipCall *call = calls_->find(callId);
    return call != nullptr && call->sendDtmfDigits(digits, method);
}

bool CallController::transferBlind(int callId, const QString &uri) {
    SipCall *call = calls_->find(callId);
    if (call == nullptr) {
        return false;
    }
    const QString targetUri = uriResolver_ ? uriResolver_(call->accountId(), uri) : uri;
    if (targetUri.isEmpty()) {
        return false;
    }
    try {
        pj::CallOpParam prm;
        call->xfer(targetUri.toStdString(), prm);
        return true;
    } catch (const pj::Error &) {
        return false;
    }
}

bool CallController::transferAttended(int callId, int targetCallId) {
    if (callId == targetCallId) {
        return false;
    }
    SipCall *call = calls_->find(callId);
    SipCall *target = calls_->find(targetCallId);
    if (call == nullptr || target == nullptr) {
        return false;
    }
    try {
        pj::CallOpParam prm;
        call->xferReplaces(*target, prm);
        return true;
    } catch (const pj::Error &) {
        return false;
    }
}

bool CallController::conference(const QList<int> &callIds) {
    QList<int> affected;
    if (!conference_->conference(callIds, *calls_, &affected)) {
        return false;
    }
    for (const int id : affected) {
        notifySnapshotChanged(id);
    }
    return true;
}

bool CallController::unconference(int callId) {
    QList<int> affected;
    if (!conference_->unconference(callId, *calls_, &affected)) {
        return false;
    }
    for (const int id : affected) {
        notifySnapshotChanged(id);
    }
    return true;
}

bool CallController::startRecording(int callId, const QString &wavPath) {
    SipCall *call = calls_->find(callId);
    if (call == nullptr || !recorder_->start(callId, *call, wavPath)) {
        return false;
    }
    notifySnapshotChanged(callId);
    return true;
}

void CallController::stopRecording(int callId) {
    SipCall *call = calls_->find(callId);
    if (recorder_->stop(callId, call) && call != nullptr) {
        notifySnapshotChanged(callId);
    }
}

bool CallController::setCallVolume(int callId, float rxLevel, float txLevel) {
    SipCall *call = calls_->find(callId);
    if (call == nullptr || !call->setVolume(rxLevel, txLevel)) {
        return false;
    }
    notifySnapshotChanged(callId);
    return true;
}

QList<CallInfo> CallController::calls() const {
    QList<CallInfo> list = calls_->snapshots();
    for (CallInfo &info : list) {
        info = enrichCallInfo(std::move(info));
    }
    return list;
}

bool CallController::isEmpty() const { return calls_->isEmpty(); }

void CallController::stopAllMedia() {
    recorder_->stopAll();
    conference_->clear();
}

void CallController::clearAll() { calls_->clear(); }

CallInfo CallController::enrichCallInfo(CallInfo info) const {
    info.recording = recorder_->isRecording(info.callId);
    info.inConference = conference_->isMember(info.callId);
    return info;
}

void CallController::notifySnapshotChanged(int callId) {
    SipCall *call = calls_->find(callId);
    if (call == nullptr) {
        return;
    }
    try {
        events_.onCallChanged(enrichCallInfo(call->snapshot()));
    } catch (const pj::Error &) {
    }
}

void CallController::handleChildCallOnMainThread(std::unique_ptr<SipCall> call,
                                                 const CallInfo &info) {
    calls_->insert(info.callId, std::move(call));
    events_.onCallChanged(enrichCallInfo(info));
}

void CallController::handleCallStateOnMainThread(const SipCall *call, const CallInfo &info) {
    if (info.state == CallState::Disconnected) {
        SipCall *registered = calls_->find(info.callId);
        if (registered == call) {
            (void)recorder_->stop(info.callId, registered);
            const QList<int> exConfIds = conference_->removeDisconnected(info.callId);
            calls_->eraseIfSame(info.callId, call);
            for (const int remId : exConfIds) {
                notifySnapshotChanged(remId);
            }
        }
        events_.onCallChanged(info);
        return;
    }
    events_.onCallChanged(enrichCallInfo(info));
}

void CallController::handleCallMediaOnMainThread(const SipCall *call, const CallInfo &info) {
    SipCall *registered = calls_->find(info.callId);
    if (registered != call || registered == nullptr) {
        return;
    }
    registered->applyCaptureRouting();
    recorder_->syncCallMedia(info.callId, *registered);
    conference_->syncMedia(*calls_);
    events_.onCallChanged(enrichCallInfo(info));
}

void CallController::handleTransferStatusOnMainThread(const SipCall *call, const CallInfo &info) {
    if (calls_->find(info.callId) != call) {
        return;
    }
    events_.onCallChanged(enrichCallInfo(info));
    if (info.transferStatus >= 200 && info.transferStatus < 300) {
        (void)hangup(info.callId);
    }
}

} // namespace tincan::sip
