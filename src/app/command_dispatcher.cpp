// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "app/command_dispatcher.h"

namespace tincan {

CommandDispatcher::CommandDispatcher(CallService &callService, QObject *parent)
    : QObject(parent), callService_(callService) {}

bool CommandDispatcher::dispatch(const CommandLine &command) {
    switch (command.action) {
    case CommandLine::Action::Dial:
        return dial(command.target);
    case CommandLine::Action::Answer:
        return answerFirstRinging();
    case CommandLine::Action::Hangup:
        return hangupActive();
    case CommandLine::Action::Show:
        emit showRequested();
        return true;
    case CommandLine::Action::Quit:
        emit quitRequested();
        return true;
    case CommandLine::Action::None:
    case CommandLine::Action::Minimized:
        return true;
    }
    return false;
}

bool CommandDispatcher::dial(const QString &target) {
    emit showRequested();
    return callService_.dial(target).isEmpty();
}

bool CommandDispatcher::answerFirstRinging() {
    for (const CallInfo &call : callService_.calls()) {
        if (call.isIncomingRinging()) {
            return callService_.answer(call.callId);
        }
    }
    return false;
}

bool CommandDispatcher::hangupActive() {
    const int callId = callService_.activeCallId();
    return callId >= 0 && callService_.hangup(callId);
}

} // namespace tincan
