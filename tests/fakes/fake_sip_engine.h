// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include "core/sip_engine.h"
#include "core/sip_events.h"

#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>

#include <optional>

namespace tincan::test {

class FakeSipEngine : public ISipEngine {
public:
    struct DialRecord {
        QString accountId;
        QString uri;
        int callId = -1;
    };

    struct RejectRecord {
        int callId = -1;
        int statusCode = 0;
    };

    struct RedirectRecord {
        int callId = -1;
        QString uri;
    };

    struct HoldRecord {
        int callId = -1;
        bool hold = false;
    };

    struct DtmfRecord {
        int callId = -1;
        QString digits;
        DtmfMethod method = DtmfMethod::Rfc4733;
    };

    struct TransferBlindRecord {
        int callId = -1;
        QString uri;
    };

    struct TransferAttendedRecord {
        int callId = -1;
        int targetCallId = -1;
    };

    struct RecordStartRecord {
        int callId = -1;
        QString wavPath;
    };

    struct VolumeRecord {
        int callId = -1;
        float rxLevel = 1.0f;
        float txLevel = 1.0f;
    };

    struct ToneRecord {
        Tone tone = Tone::Ringing;
        QString customWav;
    };

    explicit FakeSipEngine(ISipEvents *events = nullptr);

    void setEvents(ISipEvents *events);

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

    void injectRegistration(const RegistrationInfo &info);
    int injectIncomingCall(const QString &accountId, const QString &remoteUri,
                           const QString &displayName = {},
                           const QMap<QString, QString> &headers = {});
    void injectCallState(int callId, CallState state, int statusCode = 0,
                         const QString &reason = {});

    bool started = true;
    int udpPort = 50600;
    int nextCallId = 1;

    QMap<QString, AccountConfig> activeAccounts;
    QList<AccountConfig> addedAccounts;
    QStringList removedAccountIds;

    QList<DialRecord> dialedCalls;
    QList<int> answeredCallIds;
    QList<RejectRecord> rejectedCalls;
    QList<RedirectRecord> redirectedCalls;
    QList<int> hungupCallIds;
    QList<HoldRecord> holdOperations;
    QList<DtmfRecord> sentDtmf;
    QList<TransferBlindRecord> blindTransfers;
    QList<TransferAttendedRecord> attendedTransfers;
    QList<QList<int>> conferenceCalls;
    QList<int> unconferencedCallIds;
    QList<RecordStartRecord> startedRecordings;
    QList<int> stoppedRecordingIds;
    QList<VolumeRecord> volumeChanges;

    QList<AudioDevice> fakeDevices;
    QList<CodecInfo> fakeCodecs;
    int lastCaptureIndex = -1;
    int lastPlaybackIndex = -1;
    int lastRingIndex = -1;
    bool ecEnabled = false;
    int ecTailMs = 0;

    QList<ToneRecord> startedTones;
    int stopToneCount = 0;
    std::optional<Tone> currentTone;

private:
    void notifyUpdated(int callId);

    ISipEvents *events_ = nullptr;
    QMap<int, CallInfo> activeCalls_;
};

} // namespace tincan::test
