// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include <QString>

#include <pjsua2/media.hpp>

#include <map>
#include <memory>

namespace tincan::sip {

class SipCall;
class SipEndpoint;

class CallRecorder {
public:
    explicit CallRecorder(SipEndpoint &endpoint);
    ~CallRecorder();

    CallRecorder(const CallRecorder &) = delete;
    CallRecorder &operator=(const CallRecorder &) = delete;

    bool start(int callId, SipCall &call, const QString &wavPath);
    bool stop(int callId, SipCall *call);
    void stopAll();
    bool isRecording(int callId) const;
    // Re-links the call's audio and, unless the call is muted, the
    // microphone to its recorder. Call after media or mute changes.
    void syncCallMedia(int callId, SipCall &call);
    // True if the microphone currently feeds the call's recorder.
    bool isCaptureRecorded(int callId) const;

private:
    void routeCapture(pj::AudioMediaRecorder &recorder, const SipCall &call);

    SipEndpoint &endpoint_;
    std::map<int, std::unique_ptr<pj::AudioMediaRecorder>> recorders_;
};

} // namespace tincan::sip
