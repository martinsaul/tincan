// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include "app/account_service.h"
#include "app/call_tone_controller.h"
#include "app/incoming_call_coordinator.h"
#include "app/settings_store.h"
#include "core/call_info.h"
#include "core/sip_engine.h"
#include "core/sip_events.h"

#include <QList>
#include <QObject>
#include <QSet>
#include <QString>

namespace tincan {

class CallService : public QObject, public ISipEvents {
    Q_OBJECT

public:
    CallService(ISipEngine &engine, AccountService &accounts, SettingsStore &settings,
                QObject *parent = nullptr);

    QList<CallInfo> calls() const;
    int activeCallId() const;
    int activeConfirmedCallId() const;

    QString dial(const QString &text);
    bool answer(int callId);
    bool reject(int callId);
    bool redirect(int callId, const QString &target);
    bool hangup(int callId);
    void hangupAll();

    bool activateCall(int callId);
    bool setHold(int callId, bool hold);
    bool setMute(int callId, bool mute);
    bool sendDtmf(int callId, const QString &digits);
    bool sendDtmfToActive(const QString &digits);
    bool transferBlind(int callId, const QString &target);
    bool transferAttended(int callId, int targetCallId);
    bool conference(const QList<int> &callIds);
    bool mergeAllCalls();
    bool unconference(int callId);
    bool startRecording(int callId);
    void stopRecording(int callId);
    bool toggleRecording(int callId);
    bool setCallVolume(int callId, float rxLevel, float txLevel);

    bool isDnd() const;
    void setDnd(bool enabled);
    void reloadSettings();

    void onRegistrationChanged(const RegistrationInfo &info) override;
    void onIncomingCall(const CallInfo &info) override;
    void onCallChanged(const CallInfo &info) override;

signals:
    void callAdded(const tincan::CallInfo &info);
    void callUpdated(const tincan::CallInfo &info);
    void callRemoved(const tincan::CallInfo &info);
    void incomingCall(const tincan::CallInfo &info);
    void missedCall(const tincan::CallInfo &info);
    void activeCallChanged(int callId);
    void dtmfSent(int callId, const QString &digits);
    void dndChanged(bool enabled);
    void errorOccurred(const QString &message);

private:
    qsizetype findCallIndex(int callId) const;
    void upsertCall(const CallInfo &info);
    void markLocallyHandled(int callId);
    bool hasOtherActiveCall(int callId) const;
    void holdOtherActiveCalls(int exceptCallId);
    void setActiveCallId(int callId);
    void selectFallbackActiveCall();
    QString resolveTargetForCallId(int callId, const QString &target) const;
    QString resolveTargetForAccount(const QString &accountId, const QString &target) const;
    void onNoAnswerTimeout(int callId, const QString &accountId, const QString &target);
    void onAutoAnswerDue(int callId);

    ISipEngine &engine_;
    AccountService &accounts_;
    SettingsStore &settings_;
    CallToneController tones_;
    IncomingCallCoordinator incomingCoordinator_;
    QList<CallInfo> calls_;
    QSet<int> locallyHandledIds_;
    QSet<int> autoRecordedIds_;
    int activeCallId_ = -1;
    bool cachedDnd_ = false;
};

} // namespace tincan
