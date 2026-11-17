// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "app/call_service.h"

#include "core/sip_uri.h"

namespace tincan {

namespace {

bool isOutgoingRingback(const CallInfo &call) {
    return call.direction == CallDirection::Outgoing && call.state == CallState::Early &&
           (call.lastStatusCode == 0 || call.lastStatusCode == 180);
}

} // namespace

CallService::CallService(ISipEngine &engine, AccountService &accounts, SettingsStore &settings,
                         QObject *parent)
    : QObject(parent), engine_(engine), accounts_(accounts), settings_(settings) {}

QList<CallInfo> CallService::calls() const {
    return calls_;
}

QString CallService::dial(const QString &text) {
    const auto account = accounts_.defaultAccount();
    if (!account.has_value()) {
        const QString err = tr("No SIP account configured.");
        emit errorOccurred(err);
        return err;
    }

    const QString uri = SipUri::toSipUri(text, *account);
    if (uri.isEmpty()) {
        const QString err = tr("Invalid phone number or SIP URI.");
        emit errorOccurred(err);
        return err;
    }

    QString engineError;
    const int callId = engine_.makeCall(account->id, uri, &engineError);
    if (callId < 0) {
        const QString err =
            engineError.isEmpty() ? tr("Failed to place outgoing call.") : engineError;
        emit errorOccurred(err);
        return err;
    }

    if (findCallIndex(callId) < 0) {
        CallInfo initial;
        initial.callId = callId;
        initial.accountId = account->id;
        initial.direction = CallDirection::Outgoing;
        initial.state = CallState::Calling;
        initial.remoteUri = uri;
        calls_.append(initial);
        updateToneState();
        emit callAdded(initial);
    }

    return {};
}

bool CallService::answer(int callId) {
    locallyHandledIds_.insert(callId);
    return engine_.answer(callId);
}

bool CallService::reject(int callId) {
    locallyHandledIds_.insert(callId);
    const int statusCode = hasOtherActiveCall(callId) ? 486 : 603;
    return engine_.reject(callId, statusCode);
}

bool CallService::hangup(int callId) {
    locallyHandledIds_.insert(callId);
    return engine_.hangup(callId);
}

void CallService::hangupAll() {
    const QList<CallInfo> current = calls_;
    for (const CallInfo &c : current) {
        locallyHandledIds_.insert(c.callId);
        if (c.isIncomingRinging()) {
            (void)reject(c.callId);
        } else {
            (void)engine_.hangup(c.callId);
        }
    }
}

void CallService::onRegistrationChanged(const RegistrationInfo &info) {
    accounts_.handleRegistrationChanged(info);
}

void CallService::onIncomingCall(const CallInfo &info) {
    CallInfo normalized = info;
    normalized.direction = CallDirection::Incoming;
    if (normalized.state == CallState::Disconnected) {
        return;
    }

    const qsizetype idx = findCallIndex(normalized.callId);
    if (idx < 0) {
        calls_.append(normalized);
        updateToneState();
        emit callAdded(normalized);
    } else {
        calls_[idx] = normalized;
        updateToneState();
        emit callUpdated(normalized);
    }
    emit incomingCall(normalized);
}

void CallService::onCallChanged(const CallInfo &info) {
    const qsizetype idx = findCallIndex(info.callId);
    if (info.state == CallState::Disconnected) {
        bool missed = false;
        if (idx >= 0) {
            const CallInfo &prev = calls_[idx];
            missed = prev.isIncomingRinging() && !prev.connectedAt.isValid() &&
                     !locallyHandledIds_.contains(info.callId);
            calls_.removeAt(idx);
        }
        locallyHandledIds_.remove(info.callId);
        updateToneState();
        if (idx >= 0) {
            emit callRemoved(info);
        }
        if (missed) {
            emit missedCall(info);
        }
        return;
    }

    if (idx < 0) {
        calls_.append(info);
        updateToneState();
        emit callAdded(info);
    } else {
        calls_[idx] = info;
        updateToneState();
        emit callUpdated(info);
    }
}

qsizetype CallService::findCallIndex(int callId) const {
    for (qsizetype i = 0; i < calls_.size(); ++i) {
        if (calls_[i].callId == callId) {
            return i;
        }
    }
    return -1;
}

bool CallService::hasOtherActiveCall(int callId) const {
    for (const CallInfo &c : calls_) {
        if (c.callId != callId && c.state != CallState::Disconnected) {
            return true;
        }
    }
    return false;
}

void CallService::updateToneState() {
    bool anyRingingIncoming = false;
    bool anyOutgoingRingback = false;
    bool anyConfirmed = false;
    for (const CallInfo &c : calls_) {
        if (c.isIncomingRinging()) {
            anyRingingIncoming = true;
        }
        if (isOutgoingRingback(c)) {
            anyOutgoingRingback = true;
        }
        if (c.state == CallState::Confirmed) {
            anyConfirmed = true;
        }
    }

    std::optional<Tone> desiredTone;
    QString desiredWav;
    if (anyRingingIncoming) {
        if (calls_.size() > 1) {
            desiredTone = Tone::CallWaiting;
        } else {
            desiredTone = Tone::Ringing;
            desiredWav = settings_.generalSettings().ringtonePath;
        }
    } else if (anyOutgoingRingback && !anyConfirmed) {
        desiredTone = Tone::Ringback;
    }

    if (!desiredTone.has_value()) {
        if (activeTone_.has_value()) {
            engine_.stopTone();
            activeTone_.reset();
            activeToneWav_.clear();
        }
        return;
    }

    if (activeTone_ != desiredTone || activeToneWav_ != desiredWav) {
        engine_.startTone(*desiredTone, desiredWav);
        activeTone_ = desiredTone;
        activeToneWav_ = desiredWav;
    }
}

} // namespace tincan
