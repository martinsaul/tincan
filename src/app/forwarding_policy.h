// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include <QString>

namespace tincan {

struct ForwardingSettings {
    bool dnd = false;
    QString always;
    QString onBusy;
    QString onNoAnswer;
    int noAnswerSec = 15;
};

enum class ForwardingAction { Ring, Reject486, Redirect, RedirectAfter };

struct ForwardingDecision {
    ForwardingAction action = ForwardingAction::Ring;
    QString targetUri;
    int afterSec = 0;
};

class ForwardingPolicy {
public:
    using Settings = ForwardingSettings;
    using Action = ForwardingAction;
    using Decision = ForwardingDecision;

    ForwardingPolicy() = delete;

    static ForwardingDecision evaluate(const ForwardingSettings &settings, bool isBusy);
};

} // namespace tincan
