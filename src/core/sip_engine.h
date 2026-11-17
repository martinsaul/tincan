// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include "core/account_config.h"
#include "core/audio_device.h"
#include "core/call_info.h"
#include "core/codec_info.h"
#include "core/engine_config.h"
#include "core/tone.h"

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
    virtual bool hangup(int callId) = 0;
    virtual QList<CallInfo> calls() const = 0;

    virtual QList<AudioDevice> audioDevices() const = 0;
    virtual bool setAudioDevices(int captureIndex, int playbackIndex) = 0;
    virtual void setRingDevice(int playbackIndex) = 0;

    virtual QList<CodecInfo> codecs() const = 0;
    virtual bool setCodecPriority(const QString &id, int priority) = 0;
    virtual bool setEchoCanceller(bool enabled, int tailMs) = 0;

    virtual void startTone(Tone tone, const QString &customWav = {}) = 0;
    virtual void stopTone() = 0;
};

} // namespace tincan
