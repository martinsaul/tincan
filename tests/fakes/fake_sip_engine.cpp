// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "fake_sip_engine.h"

namespace tincan::test {

FakeSipEngine::FakeSipEngine(ISipEvents *events) : events_(events) {
    fakeDevices = {
        AudioDevice{0, QStringLiteral("Built-in Mic"), QStringLiteral("ALSA"), 2, 0},
        AudioDevice{1, QStringLiteral("Built-in Audio"), QStringLiteral("ALSA"), 0, 2},
        AudioDevice{2, QStringLiteral("USB Headset"), QStringLiteral("ALSA"), 1, 2},
    };
    fakeCodecs = {
        CodecInfo{QStringLiteral("opus/48000/2"), 130},
        CodecInfo{QStringLiteral("G722/16000/1"), 129},
        CodecInfo{QStringLiteral("PCMU/8000/1"), 128},
        CodecInfo{QStringLiteral("PCMA/8000/1"), 127},
    };
}

void FakeSipEngine::setEvents(ISipEvents *events) {
    events_ = events;
}

bool FakeSipEngine::start(const EngineConfig &, QString *) {
    started = true;
    return true;
}

void FakeSipEngine::shutdown() {
    started = false;
    activeCalls_.clear();
    activeAccounts.clear();
}

int FakeSipEngine::localUdpPort() const {
    return udpPort;
}

bool FakeSipEngine::addAccount(const AccountConfig &config, QString *error) {
    if (config.id.trimmed().isEmpty()) {
        if (error != nullptr) {
            *error = QStringLiteral("Empty account id");
        }
        return false;
    }
    activeAccounts.insert(config.id, config);
    addedAccounts.append(config);
    return true;
}

void FakeSipEngine::removeAccount(const QString &accountId) {
    activeAccounts.remove(accountId);
    removedAccountIds.append(accountId);
}

int FakeSipEngine::makeCall(const QString &accountId, const QString &uri, QString *error) {
    if (!activeAccounts.contains(accountId)) {
        if (error != nullptr) {
            *error = QStringLiteral("Unknown account");
        }
        return -1;
    }
    const int id = nextCallId++;
    dialedCalls.append(DialRecord{accountId, uri, id});

    CallInfo info;
    info.callId = id;
    info.accountId = accountId;
    info.direction = CallDirection::Outgoing;
    info.state = CallState::Calling;
    info.remoteUri = uri;
    activeCalls_.insert(id, info);
    return id;
}

bool FakeSipEngine::answer(int callId) {
    if (!activeCalls_.contains(callId)) {
        return false;
    }
    answeredCallIds.append(callId);
    return true;
}

bool FakeSipEngine::reject(int callId, int statusCode) {
    if (!activeCalls_.contains(callId)) {
        return false;
    }
    rejectedCalls.append(RejectRecord{callId, statusCode});
    return true;
}

bool FakeSipEngine::hangup(int callId) {
    if (!activeCalls_.contains(callId)) {
        return false;
    }
    hungupCallIds.append(callId);
    return true;
}

QList<CallInfo> FakeSipEngine::calls() const {
    return activeCalls_.values();
}

QList<AudioDevice> FakeSipEngine::audioDevices() const {
    return fakeDevices;
}

bool FakeSipEngine::setAudioDevices(int captureIndex, int playbackIndex) {
    lastCaptureIndex = captureIndex;
    lastPlaybackIndex = playbackIndex;
    return true;
}

void FakeSipEngine::setRingDevice(int playbackIndex) {
    lastRingIndex = playbackIndex;
}

QList<CodecInfo> FakeSipEngine::codecs() const {
    return fakeCodecs;
}

bool FakeSipEngine::setCodecPriority(const QString &id, int priority) {
    for (CodecInfo &c : fakeCodecs) {
        if (c.id == id) {
            c.priority = priority;
            return true;
        }
    }
    return false;
}

bool FakeSipEngine::setEchoCanceller(bool enabled, int tailMs) {
    ecEnabled = enabled;
    ecTailMs = enabled ? tailMs : 0;
    return true;
}

void FakeSipEngine::startTone(Tone tone, const QString &customWav) {
    startedTones.append(ToneRecord{tone, customWav});
    currentTone = tone;
}

void FakeSipEngine::stopTone() {
    ++stopToneCount;
    currentTone.reset();
}

void FakeSipEngine::injectRegistration(const RegistrationInfo &info) {
    if (events_ != nullptr) {
        events_->onRegistrationChanged(info);
    }
}

int FakeSipEngine::injectIncomingCall(const QString &accountId, const QString &remoteUri,
                                      const QString &displayName) {
    const int id = nextCallId++;
    CallInfo info;
    info.callId = id;
    info.accountId = accountId;
    info.direction = CallDirection::Incoming;
    info.state = CallState::Incoming;
    info.remoteUri = remoteUri;
    info.remoteDisplayName = displayName;
    activeCalls_.insert(id, info);
    if (events_ != nullptr) {
        events_->onIncomingCall(info);
    }
    return id;
}

void FakeSipEngine::injectCallState(int callId, CallState state, int statusCode,
                                    const QString &reason) {
    CallInfo info = activeCalls_.value(callId);
    info.callId = callId;
    info.state = state;
    info.lastStatusCode = statusCode;
    info.lastReason = reason;
    if (state == CallState::Confirmed && !info.connectedAt.isValid()) {
        info.connectedAt = QDateTime::currentDateTime();
    }
    if (state == CallState::Disconnected) {
        activeCalls_.remove(callId);
    } else {
        activeCalls_.insert(callId, info);
    }
    if (events_ != nullptr) {
        events_->onCallChanged(info);
    }
}

} // namespace tincan::test
