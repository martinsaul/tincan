// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include "core/sip_engine.h"
#include "core/sip_events.h"

#include <QString>

#include <map>
#include <memory>

namespace tincan::sip {

class CallRegistry;
class EventMarshaller;
class SipAccount;
class SipCall;
class SipEndpoint;
class TonePlayer;

class PjsipEngine : public ISipEngine {
public:
    explicit PjsipEngine(ISipEvents &events);
    ~PjsipEngine() override;

    PjsipEngine(const PjsipEngine &) = delete;
    PjsipEngine &operator=(const PjsipEngine &) = delete;

    bool start(const EngineConfig &config, QString *error) override;
    void shutdown() override;
    int localUdpPort() const override;

    bool addAccount(const AccountConfig &config, QString *error) override;
    void removeAccount(const QString &accountId) override;

    int makeCall(const QString &accountId, const QString &uri, QString *error) override;
    bool answer(int callId) override;
    bool reject(int callId, int statusCode) override;
    bool hangup(int callId) override;
    QList<CallInfo> calls() const override;

    QList<AudioDevice> audioDevices() const override;
    bool setAudioDevices(int captureIndex, int playbackIndex) override;
    void setRingDevice(int playbackIndex) override;

    QList<CodecInfo> codecs() const override;
    bool setCodecPriority(const QString &id, int priority) override;
    bool setEchoCanceller(bool enabled, int tailMs) override;

    void startTone(Tone tone, const QString &customWav = {}) override;
    void stopTone() override;

private:
    void handleIncomingCallOnMainThread(std::unique_ptr<SipCall> call, const CallInfo &info);
    void handleCallStateOnMainThread(const SipCall *call, const CallInfo &info);

    ISipEvents &events_;
    std::unique_ptr<EventMarshaller> marshaller_;
    std::unique_ptr<SipEndpoint> endpoint_;
    std::unique_ptr<TonePlayer> tonePlayer_;
    std::unique_ptr<CallRegistry> calls_;
    std::map<QString, std::unique_ptr<SipAccount>> accounts_;
};

} // namespace tincan::sip
