// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include "core/sip_events.h"

namespace tincan {

class SipEventRouter : public ISipEvents {
public:
    void setTarget(ISipEvents *target) { target_ = target; }

    void onRegistrationChanged(const RegistrationInfo &info) override {
        if (target_ != nullptr) {
            target_->onRegistrationChanged(info);
        }
    }

    void onIncomingCall(const CallInfo &info) override {
        if (target_ != nullptr) {
            target_->onIncomingCall(info);
        }
    }

    void onCallChanged(const CallInfo &info) override {
        if (target_ != nullptr) {
            target_->onCallChanged(info);
        }
    }

private:
    ISipEvents *target_ = nullptr;
};

} // namespace tincan
