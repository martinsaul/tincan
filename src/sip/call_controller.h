// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include "core/call_info.h"
#include "core/dtmf_method.h"
#include "core/sip_events.h"
#include "sip/sip_call.h"

#include <QList>
#include <QString>

#include <functional>
#include <memory>

namespace tincan::sip {

class CallRecorder;
class CallRegistry;
class ConferenceBridge;
class EventMarshaller;
class SipAccount;
class SipEndpoint;

class CallController {
public:
    using UriResolver = std::function<QString(const QString &accountId, const QString &uri)>;

    CallController(ISipEvents &events, SipEndpoint &endpoint, EventMarshaller &marshaller,
                   UriResolver uriResolver);
    ~CallController();

    CallController(const CallController &) = delete;
    CallController &operator=(const CallController &) = delete;

    SipCall::Callbacks makeCallCallbacks();
    void adoptIncomingCall(std::unique_ptr<SipCall> call, const CallInfo &info);

    int makeCall(SipAccount &account, const QString &accountId, const QString &targetUri,
                 QString *error);
    bool answer(int callId);
    bool reject(int callId, int statusCode);
    bool redirect(int callId, const QString &uri);
    bool hangup(int callId);
    bool setHold(int callId, bool hold);
    bool setMute(int callId, bool mute);
    bool sendDtmf(int callId, const QString &digits, DtmfMethod method);
    bool transferBlind(int callId, const QString &uri);
    bool transferAttended(int callId, int targetCallId);
    bool conference(const QList<int> &callIds);
    bool unconference(int callId);
    bool startRecording(int callId, const QString &wavPath);
    void stopRecording(int callId);
    bool setCallVolume(int callId, float rxLevel, float txLevel);

    QList<CallInfo> calls() const;
    bool isEmpty() const;
    void stopAllMedia();
    void clearAll();

private:
    CallInfo enrichCallInfo(CallInfo info) const;
    void notifySnapshotChanged(int callId);
    void handleChildCallOnMainThread(std::unique_ptr<SipCall> call, const CallInfo &info);
    void handleCallStateOnMainThread(const SipCall *call, const CallInfo &info);
    void handleCallMediaOnMainThread(const SipCall *call, const CallInfo &info);
    void handleTransferStatusOnMainThread(const SipCall *call, const CallInfo &info);

    ISipEvents &events_;
    SipEndpoint &endpoint_;
    EventMarshaller &marshaller_;
    UriResolver uriResolver_;
    std::unique_ptr<CallRecorder> recorder_;
    std::unique_ptr<ConferenceBridge> conference_;
    std::unique_ptr<CallRegistry> calls_;
};

} // namespace tincan::sip
