// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include "core/call_info.h"

namespace tincan {

enum class AutoAnswerMode { Off, Always, HeaderOnly };

struct AutoAnswerSettings {
    AutoAnswerMode mode = AutoAnswerMode::Off;
    int delaySec = 0;
};

struct AutoAnswerDecision {
    bool shouldAnswer = false;
    int delaySec = 0;
};

class AutoAnswerPolicy {
public:
    using Mode = AutoAnswerMode;
    using Settings = AutoAnswerSettings;
    using Decision = AutoAnswerDecision;

    AutoAnswerPolicy() = delete;

    static AutoAnswerDecision evaluate(const AutoAnswerSettings &settings, const CallInfo &call,
                                       bool hasOtherActiveCall);
};

} // namespace tincan
