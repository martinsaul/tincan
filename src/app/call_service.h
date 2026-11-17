// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include "app/account_service.h"
#include "app/settings_store.h"
#include "core/call_info.h"
#include "core/sip_engine.h"
#include "core/sip_events.h"
#include "core/tone.h"

#include <QList>
#include <QObject>
#include <QSet>
#include <QString>

#include <optional>

namespace tincan {

class CallService : public QObject, public ISipEvents {
    Q_OBJECT

public:
    CallService(ISipEngine &engine, AccountService &accounts, SettingsStore &settings,
                QObject *parent = nullptr);

    QList<CallInfo> calls() const;

    QString dial(const QString &text);
    bool answer(int callId);
    bool reject(int callId);
    bool hangup(int callId);
    void hangupAll();

    void onRegistrationChanged(const RegistrationInfo &info) override;
    void onIncomingCall(const CallInfo &info) override;
    void onCallChanged(const CallInfo &info) override;

signals:
    void callAdded(const tincan::CallInfo &info);
    void callUpdated(const tincan::CallInfo &info);
    void callRemoved(const tincan::CallInfo &info);
    void incomingCall(const tincan::CallInfo &info);
    void missedCall(const tincan::CallInfo &info);
    void errorOccurred(const QString &message);

private:
    qsizetype findCallIndex(int callId) const;
    bool hasOtherActiveCall(int callId) const;
    void updateToneState();

    ISipEngine &engine_;
    AccountService &accounts_;
    SettingsStore &settings_;
    QList<CallInfo> calls_;
    QSet<int> locallyHandledIds_;
    std::optional<Tone> activeTone_;
    QString activeToneWav_;
};

} // namespace tincan
