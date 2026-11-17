// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "recording_events.h"

namespace tincan::test {

RecordingEvents::RecordingEvents(QObject *parent) : QObject(parent) {}

void RecordingEvents::onRegistrationChanged(const RegistrationInfo &info) {
    registrations_.push_back(info);
    emit registrationChanged();
}

void RecordingEvents::onIncomingCall(const CallInfo &info) {
    incomingCalls_.push_back(info);
    emit incomingCallReceived();
}

void RecordingEvents::onCallChanged(const CallInfo &info) {
    callChanges_.push_back(info);
    emit callChanged();
}

void RecordingEvents::onPresenceChanged(const PresenceInfo &info) {
    presenceChanges_.push_back(info);
    emit presenceChanged();
}

void RecordingEvents::onIncomingMessage(const MessageInfo &info) {
    incomingMessages_.push_back(info);
    emit incomingMessageReceived();
}

void RecordingEvents::onMessageStatus(quint64 token, int statusCode, const QString &reason) {
    messageStatuses_.push_back(MessageStatusEvent{token, statusCode, reason});
    emit messageStatusReceived();
}

void RecordingEvents::clear() {
    registrations_.clear();
    incomingCalls_.clear();
    callChanges_.clear();
    presenceChanges_.clear();
    incomingMessages_.clear();
    messageStatuses_.clear();
}

const QList<RegistrationInfo> &RecordingEvents::registrations() const { return registrations_; }
const QList<CallInfo> &RecordingEvents::incomingCalls() const { return incomingCalls_; }
const QList<CallInfo> &RecordingEvents::callChanges() const { return callChanges_; }
const QList<PresenceInfo> &RecordingEvents::presenceChanges() const { return presenceChanges_; }
const QList<MessageInfo> &RecordingEvents::incomingMessages() const { return incomingMessages_; }
const QList<RecordingEvents::MessageStatusEvent> &RecordingEvents::messageStatuses() const {
    return messageStatuses_;
}

bool RecordingEvents::hasCallState(int callId, CallState state) const {
    return findCallState(callId, state).has_value();
}

std::optional<CallInfo> RecordingEvents::findCallState(int callId, CallState state) const {
    for (auto it = callChanges_.crbegin(); it != callChanges_.crend(); ++it) {
        if (it->callId == callId && it->state == state) {
            return *it;
        }
    }
    return std::nullopt;
}

} // namespace tincan::test
