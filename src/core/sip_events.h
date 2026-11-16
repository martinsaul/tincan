// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include "core/call_info.h"
#include "core/registration_info.h"

namespace tincan {

class ISipEvents {
public:
    virtual ~ISipEvents() = default;
    virtual void onRegistrationChanged(const RegistrationInfo &info) = 0;
    virtual void onIncomingCall(const CallInfo &info) = 0;
    virtual void onCallChanged(const CallInfo &info) = 0;
};

} // namespace tincan
