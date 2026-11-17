// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "app/incoming_call_coordinator.h"

#include "app/auto_answer_policy.h"
#include "app/forwarding_policy.h"

#include <QTimer>

#include <algorithm>

namespace tincan {

IncomingCallCoordinator::IncomingCallCoordinator(SettingsStore &settings, QObject *parent)
    : QObject(parent), settings_(settings) {}

IncomingCallCoordinator::Evaluation IncomingCallCoordinator::handleIncoming(const CallInfo &call,
                                                                            bool isBusy) {
    cancelTimers(call.callId);

    const CallSettings cs = settings_.callSettings();
    const ForwardingSettings fwdCfg{cs.dnd, cs.forwardAlways, cs.forwardOnBusy,
                                    cs.forwardOnNoAnswer, cs.forwardNoAnswerSec};
    const ForwardingDecision fwd = ForwardingPolicy::evaluate(fwdCfg, isBusy);

    if (fwd.action == ForwardingAction::Reject486) {
        return {ImmediateOutcome::Rejected486, {}};
    }
    if (fwd.action == ForwardingAction::Redirect) {
        return {ImmediateOutcome::Redirected, fwd.targetUri};
    }
    if (fwd.action == ForwardingAction::RedirectAfter) {
        scheduleNoAnswer(call.callId, call.accountId, fwd.targetUri, fwd.afterSec);
    }

    const AutoAnswerSettings aaCfg{cs.autoAnswerMode, cs.autoAnswerDelaySec};
    const AutoAnswerDecision aa = AutoAnswerPolicy::evaluate(aaCfg, call, isBusy);
    if (aa.shouldAnswer) {
        scheduleAutoAnswer(call.callId, aa.delaySec);
    }

    return {ImmediateOutcome::Ring, {}};
}

void IncomingCallCoordinator::cancelTimers(int callId) {
    if (QTimer *t = noAnswerTimers_.take(callId)) {
        t->stop();
        t->deleteLater();
    }
    if (QTimer *t = autoAnswerTimers_.take(callId)) {
        t->stop();
        t->deleteLater();
    }
}

void IncomingCallCoordinator::scheduleNoAnswer(int callId, const QString &accountId,
                                               const QString &target, int delaySec) {
    auto *timer = new QTimer(this);
    timer->setSingleShot(true);
    noAnswerTimers_.insert(callId, timer);
    connect(timer, &QTimer::timeout, this, [this, callId, accountId, target]() {
        if (QTimer *t = noAnswerTimers_.take(callId)) {
            t->deleteLater();
        }
        emit noAnswerTimeout(callId, accountId, target);
    });
    timer->start(std::max(0, delaySec) * 1000);
}

void IncomingCallCoordinator::scheduleAutoAnswer(int callId, int delaySec) {
    auto *timer = new QTimer(this);
    timer->setSingleShot(true);
    autoAnswerTimers_.insert(callId, timer);
    connect(timer, &QTimer::timeout, this, [this, callId]() {
        if (QTimer *t = autoAnswerTimers_.take(callId)) {
            t->deleteLater();
        }
        emit autoAnswerDue(callId);
    });
    timer->start(std::max(0, delaySec) * 1000);
}

} // namespace tincan
