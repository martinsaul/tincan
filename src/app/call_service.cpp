// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "app/call_service.h"

#include "app/recording_namer.h"
#include "core/sip_uri.h"

namespace tincan {

namespace {

bool isAnsweredElsewhere(const CallInfo &info) {
    return info.lastStatusCode == 200 ||
           info.lastReason.contains(QStringLiteral("elsewhere"), Qt::CaseInsensitive);
}

} // namespace

CallService::CallService(ISipEngine &engine, AccountService &accounts, SettingsStore &settings,
                         QObject *parent)
    : QObject(parent), engine_(engine), accounts_(accounts), settings_(settings),
      tones_(engine, settings), incomingCoordinator_(settings, this),
      cachedDnd_(settings.callSettings().dnd) {
    connect(&incomingCoordinator_, &IncomingCallCoordinator::noAnswerTimeout, this,
            &CallService::onNoAnswerTimeout);
    connect(&incomingCoordinator_, &IncomingCallCoordinator::autoAnswerDue, this,
            &CallService::onAutoAnswerDue);
}

QList<CallInfo> CallService::calls() const { return calls_; }
int CallService::activeCallId() const { return activeCallId_; }

int CallService::activeConfirmedCallId() const {
    const qsizetype idx = findCallIndex(activeCallId_);
    if (idx >= 0 && calls_[idx].state == CallState::Confirmed) {
        return activeCallId_;
    }
    for (const CallInfo &c : calls_) {
        if (c.state == CallState::Confirmed && !c.localHold) {
            return c.callId;
        }
    }
    for (const CallInfo &c : calls_) {
        if (c.state == CallState::Confirmed) {
            return c.callId;
        }
    }
    return -1;
}

QString CallService::dial(const QString &text) { return dialOnAccount({}, text); }

QString CallService::dialOnAccount(const QString &accountId, const QString &text) {
    const auto account =
        accountId.trimmed().isEmpty() ? accounts_.defaultAccount() : accounts_.account(accountId);
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
    holdOtherActiveCalls(-1);
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
        initial.remoteUri = uri;
        upsertCall(initial);
    }
    setActiveCallId(callId);
    return {};
}

bool CallService::answer(int callId) {
    markLocallyHandled(callId);
    holdOtherActiveCalls(callId);
    if (!engine_.answer(callId)) {
        return false;
    }
    setActiveCallId(callId);
    return true;
}

bool CallService::reject(int callId) {
    markLocallyHandled(callId);
    return engine_.reject(callId, hasOtherActiveCall(callId) ? 486 : 603);
}

bool CallService::redirect(int callId, const QString &target) {
    markLocallyHandled(callId);
    const QString uri = resolveTargetForCallId(callId, target);
    return !uri.isEmpty() && engine_.redirect(callId, uri);
}

bool CallService::hangup(int callId) {
    markLocallyHandled(callId);
    return engine_.hangup(callId);
}

void CallService::hangupAll() {
    const QList<CallInfo> current = calls_;
    for (const CallInfo &c : current) {
        (void)(c.isIncomingRinging() ? reject(c.callId) : hangup(c.callId));
    }
}

bool CallService::activateCall(int callId) {
    const qsizetype idx = findCallIndex(callId);
    if (idx < 0) {
        return false;
    }
    const CallInfo target = calls_[idx];
    if (!target.isIncomingRinging()) {
        if (!target.inConference) {
            holdOtherActiveCalls(callId);
        }
        if (target.localHold) {
            (void)engine_.setHold(callId, false);
        }
    }
    setActiveCallId(callId);
    return true;
}

bool CallService::setHold(int callId, bool hold) {
    const qsizetype idx = findCallIndex(callId);
    if (!hold && idx >= 0 && !calls_[idx].inConference) {
        holdOtherActiveCalls(callId);
    }
    const bool ok = engine_.setHold(callId, hold);
    if (ok && !hold) {
        setActiveCallId(callId);
    }
    return ok;
}

bool CallService::sendDtmf(int callId, const QString &digits) {
    if (digits.isEmpty() || !engine_.sendDtmf(callId, digits, settings_.callSettings().dtmfMethod)) {
        return false;
    }
    emit dtmfSent(callId, digits);
    return true;
}

bool CallService::sendDtmfToActive(const QString &digits) {
    const int id = activeConfirmedCallId();
    return id >= 0 && sendDtmf(id, digits);
}

bool CallService::transferBlind(int callId, const QString &target) {
    const QString uri = resolveTargetForCallId(callId, target);
    return !uri.isEmpty() && engine_.transferBlind(callId, uri);
}

bool CallService::transferAttended(int callId, int targetCallId) {
    return engine_.transferAttended(callId, targetCallId);
}
bool CallService::conference(const QList<int> &callIds) { return engine_.conference(callIds); }
bool CallService::unconference(int callId) { return engine_.unconference(callId); }
void CallService::stopRecording(int callId) { engine_.stopRecording(callId); }
bool CallService::isDnd() const { return settings_.callSettings().dnd; }
bool CallService::setMute(int callId, bool mute) { return engine_.setMute(callId, mute); }

bool CallService::mergeAllCalls() {
    QList<int> ids;
    for (const CallInfo &c : calls_) {
        if (c.state == CallState::Confirmed) {
            ids.append(c.callId);
        }
    }
    return ids.size() >= 2 && engine_.conference(ids);
}

bool CallService::startRecording(int callId) {
    const qsizetype idx = findCallIndex(callId);
    if (idx < 0) {
        return false;
    }
    const QString wavPath =
        RecordingNamer::buildWavPath(settings_.callSettings().recordingsDir, calls_[idx].remoteUri);
    return engine_.startRecording(callId, wavPath);
}

bool CallService::toggleRecording(int callId) {
    const qsizetype idx = findCallIndex(callId);
    if (idx >= 0 && calls_[idx].recording) {
        engine_.stopRecording(callId);
        return true;
    }
    return idx >= 0 && startRecording(callId);
}

bool CallService::setCallVolume(int callId, float rxLevel, float txLevel) {
    return engine_.setCallVolume(callId, rxLevel, txLevel);
}

void CallService::setDnd(bool enabled) {
    CallSettings cs = settings_.callSettings();
    if (cs.dnd == enabled && cachedDnd_ == enabled) {
        return;
    }
    cs.dnd = enabled;
    cachedDnd_ = enabled;
    settings_.setCallSettings(cs);
    emit dndChanged(enabled);
}

void CallService::reloadSettings() {
    const bool currentDnd = settings_.callSettings().dnd;
    if (cachedDnd_ != currentDnd) {
        cachedDnd_ = currentDnd;
        emit dndChanged(currentDnd);
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
    const auto eval =
        incomingCoordinator_.handleIncoming(normalized, hasOtherActiveCall(normalized.callId));
    if (eval.outcome == IncomingCallCoordinator::ImmediateOutcome::Rejected486) {
        locallyHandledIds_.insert(normalized.callId);
        (void)engine_.reject(normalized.callId, 486);
        return;
    }
    if (eval.outcome == IncomingCallCoordinator::ImmediateOutcome::Redirected) {
        locallyHandledIds_.insert(normalized.callId);
        (void)engine_.redirect(normalized.callId,
                               resolveTargetForAccount(normalized.accountId, eval.redirectTarget));
        return;
    }
    upsertCall(normalized);
    emit incomingCall(normalized);
}

void CallService::onCallChanged(const CallInfo &info) {
    if (info.state == CallState::Disconnected) {
        handleDisconnected(info);
        return;
    }
    if (!info.isIncomingRinging()) {
        incomingCoordinator_.cancelTimers(info.callId);
    }
    upsertCall(info);
    if (info.state == CallState::Confirmed && settings_.callSettings().autoRecord &&
        !info.recording && !autoRecordedIds_.contains(info.callId)) {
        autoRecordedIds_.insert(info.callId);
        (void)startRecording(info.callId);
    }
}

void CallService::handleDisconnected(const CallInfo &info) {
    const qsizetype idx = findCallIndex(info.callId);
    incomingCoordinator_.cancelTimers(info.callId);
    autoRecordedIds_.remove(info.callId);
    CallInfo finalInfo = info;
    if (idx >= 0 && !finalInfo.connectedAt.isValid()) {
        finalInfo.connectedAt = calls_[idx].connectedAt;
    }
    const bool wasHandled = locallyHandledIds_.remove(info.callId);
    const bool missed = idx >= 0 && calls_[idx].isIncomingRinging() &&
                        !finalInfo.connectedAt.isValid() && !wasHandled &&
                        !isAnsweredElsewhere(finalInfo);
    if (idx >= 0) {
        calls_.removeAt(idx);
    }
    if (activeCallId_ == info.callId) {
        selectFallbackActiveCall();
    }
    tones_.update(calls_);
    if (idx >= 0) {
        emit callRemoved(finalInfo);
        emit callEnded(finalInfo, missed);
    }
    if (missed) {
        emit missedCall(finalInfo);
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

void CallService::upsertCall(const CallInfo &info) {
    const qsizetype idx = findCallIndex(info.callId);
    if (idx < 0) {
        calls_.append(info);
        if (activeCallId_ < 0) {
            setActiveCallId(info.callId);
        }
        tones_.update(calls_);
        emit callAdded(info);
        return;
    }
    calls_[idx] = info;
    tones_.update(calls_);
    emit callUpdated(info);
}

void CallService::markLocallyHandled(int callId) {
    incomingCoordinator_.cancelTimers(callId);
    locallyHandledIds_.insert(callId);
}

bool CallService::hasOtherActiveCall(int callId) const {
    for (const CallInfo &c : calls_) {
        if (c.callId != callId && c.state != CallState::Disconnected) {
            return true;
        }
    }
    return false;
}

void CallService::holdOtherActiveCalls(int exceptCallId) {
    const QList<CallInfo> snapshot = calls_;
    for (const CallInfo &c : snapshot) {
        if (c.callId != exceptCallId && c.state == CallState::Confirmed && !c.localHold &&
            !c.inConference) {
            (void)engine_.setHold(c.callId, true);
        }
    }
}

void CallService::setActiveCallId(int callId) {
    if (activeCallId_ != callId) {
        activeCallId_ = callId;
        emit activeCallChanged(callId);
    }
}

void CallService::selectFallbackActiveCall() {
    for (const CallInfo &c : calls_) {
        if (c.state == CallState::Confirmed && !c.localHold) {
            setActiveCallId(c.callId);
            return;
        }
    }
    setActiveCallId(calls_.isEmpty() ? -1 : calls_.first().callId);
}

QString CallService::resolveTargetForCallId(int callId, const QString &target) const {
    const qsizetype idx = findCallIndex(callId);
    return resolveTargetForAccount(idx >= 0 ? calls_[idx].accountId : QString{}, target);
}

QString CallService::resolveTargetForAccount(const QString &accountId,
                                             const QString &target) const {
    for (const AccountConfig &acc : accounts_.accounts()) {
        if (acc.id == accountId) {
            return SipUri::toSipUri(target, acc);
        }
    }
    if (const auto def = accounts_.defaultAccount(); def.has_value()) {
        return SipUri::toSipUri(target, *def);
    }
    return SipUri::toSipUri(target, AccountConfig{});
}

void CallService::onNoAnswerTimeout(int callId, const QString &accountId, const QString &target) {
    const qsizetype idx = findCallIndex(callId);
    if (idx >= 0 && calls_[idx].isIncomingRinging()) {
        locallyHandledIds_.insert(callId);
        (void)engine_.redirect(callId, resolveTargetForAccount(accountId, target));
    }
}

void CallService::onAutoAnswerDue(int callId) {
    const qsizetype idx = findCallIndex(callId);
    if (idx >= 0 && calls_[idx].isIncomingRinging() && !hasOtherActiveCall(callId)) {
        (void)answer(callId);
    }
}

} // namespace tincan
