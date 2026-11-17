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
    void syncCallMedia(int callId, SipCall &call);

private:
    SipEndpoint &endpoint_;
    std::map<int, std::unique_ptr<pj::AudioMediaRecorder>> recorders_;
};

} // namespace tincan::sip
