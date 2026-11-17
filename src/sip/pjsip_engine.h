// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include "core/sip_engine.h"
#include "core/sip_events.h"

#include <QString>

#include <map>
#include <memory>

namespace tincan::sip {

class BuddyRegistry;
class CallController;
class EventMarshaller;
class SipAccount;
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
    bool redirect(int callId, const QString &uri) override;
    bool hangup(int callId) override;
    bool setHold(int callId, bool hold) override;
    bool setMute(int callId, bool mute) override;
    bool sendDtmf(int callId, const QString &digits, DtmfMethod method) override;
    bool transferBlind(int callId, const QString &uri) override;
    bool transferAttended(int callId, int targetCallId) override;
    bool conference(const QList<int> &callIds) override;
    bool unconference(int callId) override;
    bool startRecording(int callId, const QString &wavPath) override;
    void stopRecording(int callId) override;
    bool setCallVolume(int callId, float rxLevel, float txLevel) override;
    QList<CallInfo> calls() const override;

    QList<AudioDevice> audioDevices() const override;
    bool setAudioDevices(int captureIndex, int playbackIndex) override;
    void setRingDevice(int playbackIndex) override;

    QList<CodecInfo> codecs() const override;
    bool setCodecPriority(const QString &id, int priority) override;
    bool setEchoCanceller(bool enabled, int tailMs) override;

    void startTone(Tone tone, const QString &customWav = {}) override;
    void stopTone() override;

    bool subscribePresence(const QString &accountId, const QString &uri) override;
    void unsubscribePresence(const QString &uri) override;
    bool setOwnPresence(const QString &accountId, PresenceState state,
                        const QString &note = {}) override;
    bool sendMessage(const QString &accountId, const QString &uri, const QString &body,
                     quint64 token) override;

private:
    QString resolveUriForAccount(const QString &accountId, const QString &uri) const;

    ISipEvents &events_;
    std::unique_ptr<EventMarshaller> marshaller_;
    std::unique_ptr<SipEndpoint> endpoint_;
    std::unique_ptr<TonePlayer> tonePlayer_;
    std::unique_ptr<CallController> callController_;
    std::unique_ptr<BuddyRegistry> buddyRegistry_;
    std::map<QString, std::unique_ptr<SipAccount>> accounts_;
};

} // namespace tincan::sip
