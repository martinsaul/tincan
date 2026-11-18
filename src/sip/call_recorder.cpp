// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "sip/call_recorder.h"

#include "sip/sip_call.h"
#include "sip/sip_endpoint.h"

#include <QDir>
#include <QFileInfo>

#include <algorithm>

namespace tincan::sip {

CallRecorder::CallRecorder(SipEndpoint &endpoint) : endpoint_(endpoint) {}

CallRecorder::~CallRecorder() {
    stopAll();
}

bool CallRecorder::start(int callId, SipCall &call, const QString &wavPath) {
    const QString trimmed = wavPath.trimmed();
    if (!endpoint_.isStarted() || trimmed.isEmpty()) {
        return false;
    }

    (void)stop(callId, &call);

    const QFileInfo fileInfo(trimmed);
    if (!QDir().mkpath(fileInfo.absolutePath())) {
        return false;
    }

    try {
        auto recorder = std::make_unique<pj::AudioMediaRecorder>();
        recorder->createRecorder(trimmed.toStdString());
        routeCapture(*recorder, call);

        pj::AudioMedia callAudio;
        if (call.tryGetAudioMedia(&callAudio)) {
            callAudio.startTransmit(*recorder);
        }

        recorders_[callId] = std::move(recorder);
        return true;
    } catch (const pj::Error &) {
        return false;
    }
}

bool CallRecorder::stop(int callId, SipCall *call) {
    const auto it = recorders_.find(callId);
    if (it == recorders_.end()) {
        return false;
    }

    if (endpoint_.isStarted()) {
        try {
            endpoint_.pjEndpoint().audDevManager().getCaptureDevMedia().stopTransmit(*it->second);
        } catch (const pj::Error &) {
        }
        if (call != nullptr) {
            pj::AudioMedia callAudio;
            if (call->tryGetAudioMedia(&callAudio)) {
                try {
                    callAudio.stopTransmit(*it->second);
                } catch (const pj::Error &) {
                }
            }
        }
    }

    recorders_.erase(it);
    return true;
}

void CallRecorder::stopAll() {
    if (endpoint_.isStarted()) {
        for (auto &[id, rec] : recorders_) {
            (void)id;
            try {
                endpoint_.pjEndpoint().audDevManager().getCaptureDevMedia().stopTransmit(*rec);
            } catch (const pj::Error &) {
            }
        }
    }
    recorders_.clear();
}

bool CallRecorder::isRecording(int callId) const {
    return recorders_.find(callId) != recorders_.end();
}

void CallRecorder::syncCallMedia(int callId, SipCall &call) {
    const auto it = recorders_.find(callId);
    if (it == recorders_.end() || !endpoint_.isStarted()) {
        return;
    }

    routeCapture(*it->second, call);
    pj::AudioMedia callAudio;
    if (call.tryGetAudioMedia(&callAudio)) {
        try {
            callAudio.startTransmit(*it->second);
        } catch (const pj::Error &) {
        }
    }
}

void CallRecorder::routeCapture(pj::AudioMediaRecorder &recorder, const SipCall &call) {
    // The recording follows what the far end hears from us: muted spans are
    // silent on our side instead of capturing the microphone.
    try {
        pj::AudioMedia &capture = endpoint_.pjEndpoint().audDevManager().getCaptureDevMedia();
        if (call.isMuted()) {
            capture.stopTransmit(recorder);
        } else {
            capture.startTransmit(recorder);
        }
    } catch (const pj::Error &) {
    }
}

bool CallRecorder::isCaptureRecorded(int callId) const {
    const auto it = recorders_.find(callId);
    if (it == recorders_.end() || !endpoint_.isStarted()) {
        return false;
    }
    try {
        const int recorderPort = it->second->getPortId();
        const pj::ConfPortInfo capture =
            endpoint_.pjEndpoint().audDevManager().getCaptureDevMedia().getPortInfo();
        return std::find(capture.listeners.begin(), capture.listeners.end(), recorderPort) !=
               capture.listeners.end();
    } catch (const pj::Error &) {
        return false;
    }
}

} // namespace tincan::sip
