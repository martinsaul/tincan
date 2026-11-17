// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include "core/call_info.h"
#include "core/message_info.h"
#include "core/presence_info.h"
#include "core/registration_info.h"

namespace tincan {

class ISipEvents {
public:
    virtual ~ISipEvents() = default;
    virtual void onRegistrationChanged(const RegistrationInfo &info) { (void)info; }
    virtual void onIncomingCall(const CallInfo &info) { (void)info; }
    virtual void onCallChanged(const CallInfo &info) { (void)info; }
    virtual void onPresenceChanged(const PresenceInfo &info) { (void)info; }
    virtual void onIncomingMessage(const MessageInfo &info) { (void)info; }
    virtual void onMessageStatus(quint64 token, int statusCode, const QString &reason) {
        (void)token;
        (void)statusCode;
        (void)reason;
    }
};

} // namespace tincan
