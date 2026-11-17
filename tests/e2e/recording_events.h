// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include "core/sip_events.h"

#include <QList>
#include <QObject>
#include <QString>

#include <optional>

namespace tincan::test {

class RecordingEvents : public QObject, public ISipEvents {
    Q_OBJECT

public:
    struct MessageStatusEvent {
        quint64 token = 0;
        int statusCode = 0;
        QString reason;
    };

    explicit RecordingEvents(QObject *parent = nullptr);
    ~RecordingEvents() override = default;

    void onRegistrationChanged(const RegistrationInfo &info) override;
    void onIncomingCall(const CallInfo &info) override;
    void onCallChanged(const CallInfo &info) override;
    void onPresenceChanged(const PresenceInfo &info) override;
    void onIncomingMessage(const MessageInfo &info) override;
    void onMessageStatus(quint64 token, int statusCode, const QString &reason) override;

    void clear();

    const QList<RegistrationInfo> &registrations() const;
    const QList<CallInfo> &incomingCalls() const;
    const QList<CallInfo> &callChanges() const;
    const QList<PresenceInfo> &presenceChanges() const;
    const QList<MessageInfo> &incomingMessages() const;
    const QList<MessageStatusEvent> &messageStatuses() const;

    bool hasCallState(int callId, CallState state) const;
    std::optional<CallInfo> findCallState(int callId, CallState state) const;

signals:
    void registrationChanged();
    void incomingCallReceived();
    void callChanged();
    void presenceChanged();
    void incomingMessageReceived();
    void messageStatusReceived();

private:
    QList<RegistrationInfo> registrations_;
    QList<CallInfo> incomingCalls_;
    QList<CallInfo> callChanges_;
    QList<PresenceInfo> presenceChanges_;
    QList<MessageInfo> incomingMessages_;
    QList<MessageStatusEvent> messageStatuses_;
};

} // namespace tincan::test
