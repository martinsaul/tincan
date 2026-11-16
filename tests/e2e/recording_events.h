// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include "core/sip_events.h"

#include <QList>
#include <QObject>

#include <optional>

namespace tincan::test {

class RecordingEvents : public QObject, public ISipEvents {
    Q_OBJECT

public:
    explicit RecordingEvents(QObject *parent = nullptr);
    ~RecordingEvents() override = default;

    void onRegistrationChanged(const RegistrationInfo &info) override;
    void onIncomingCall(const CallInfo &info) override;
    void onCallChanged(const CallInfo &info) override;

    void clear();

    const QList<RegistrationInfo> &registrations() const;
    const QList<CallInfo> &incomingCalls() const;
    const QList<CallInfo> &callChanges() const;

    bool hasCallState(int callId, CallState state) const;
    std::optional<CallInfo> findCallState(int callId, CallState state) const;

signals:
    void registrationChanged();
    void incomingCallReceived();
    void callChanged();

private:
    QList<RegistrationInfo> registrations_;
    QList<CallInfo> incomingCalls_;
    QList<CallInfo> callChanges_;
};

} // namespace tincan::test
