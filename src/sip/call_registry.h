// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include "core/call_info.h"
#include "sip/sip_call.h"

#include <QList>

#include <map>
#include <memory>

namespace tincan::sip {

class CallRegistry {
public:
    CallRegistry() = default;
    ~CallRegistry() = default;

    CallRegistry(const CallRegistry &) = delete;
    CallRegistry &operator=(const CallRegistry &) = delete;

    void insert(int callId, std::unique_ptr<SipCall> call);
    // The live call for `callId`; a disconnected call awaiting removal is not
    // returned, because pjsua may already have given its id to a new call.
    SipCall *find(int callId) const;
    // True if the slot holds exactly `call`, live or disconnected.
    bool holds(int callId, const SipCall *call) const;
    void erase(int callId);
    // Erases only if the slot still holds `call` (PJSIP reuses call ids).
    void eraseIfSame(int callId, const SipCall *call);
    void clear();
    bool isEmpty() const;
    QList<CallInfo> snapshots() const;

private:
    std::map<int, std::unique_ptr<SipCall>> calls_;
};

} // namespace tincan::sip
