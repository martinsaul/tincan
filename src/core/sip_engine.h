// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include "core/account_config.h"
#include "core/audio_device.h"
#include "core/call_info.h"
#include "core/codec_info.h"
#include "core/dtmf_method.h"
#include "core/engine_config.h"
#include "core/presence_info.h"
#include "core/tone.h"

#include <QChar>
#include <QList>
#include <QString>

namespace tincan {

class ISipEngine {
public:
    virtual ~ISipEngine() = default;
    virtual bool start(const EngineConfig &config, QString *error) = 0;
    virtual void shutdown() = 0;
    virtual int localUdpPort() const = 0;
    virtual bool addAccount(const AccountConfig &config, QString *error) = 0;
    virtual void removeAccount(const QString &accountId) = 0;
    virtual int makeCall(const QString &accountId, const QString &uri, QString *error) = 0;
    virtual bool answer(int callId) = 0;
    virtual bool reject(int callId, int statusCode) = 0;
    virtual bool redirect(int callId, const QString &uri) = 0;
    virtual bool hangup(int callId) = 0;
    virtual bool setHold(int callId, bool hold) = 0;
    virtual bool setMute(int callId, bool mute) = 0;
    virtual bool sendDtmf(int callId, const QString &digits, DtmfMethod method) = 0;
    virtual bool transferBlind(int callId, const QString &uri) = 0;
    virtual bool transferAttended(int callId, int targetCallId) = 0;
    virtual bool conference(const QList<int> &callIds) = 0;
    virtual bool unconference(int callId) = 0;
    virtual bool startRecording(int callId, const QString &wavPath) = 0;
    virtual void stopRecording(int callId) = 0;
    virtual bool setCallVolume(int callId, float rxLevel, float txLevel) = 0;
    virtual QList<CallInfo> calls() const = 0;

    virtual QList<AudioDevice> audioDevices() const = 0;
    virtual bool setAudioDevices(int captureIndex, int playbackIndex) = 0;
    virtual void setRingDevice(int playbackIndex) = 0;

    virtual QList<CodecInfo> codecs() const = 0;
    virtual bool setCodecPriority(const QString &id, int priority) = 0;
    virtual bool setEchoCanceller(bool enabled, int tailMs) = 0;

    virtual void startTone(Tone tone, const QString &customWav = {}) = 0;
    virtual void stopTone() = 0;
    // Short local feedback burst for a keypad digit. Plays on the speaker only;
    // it is never mixed into a call, the microphone path or a recording.
    virtual void playKeypadTone(QChar digit) = 0;

    virtual bool subscribePresence(const QString &accountId, const QString &uri) = 0;
    virtual void unsubscribePresence(const QString &uri) = 0;
    virtual bool setOwnPresence(const QString &accountId, PresenceState state,
                                const QString &note = {}) = 0;
    virtual bool sendMessage(const QString &accountId, const QString &uri, const QString &body,
                             quint64 token) = 0;
};

} // namespace tincan
