// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "app/forwarding_policy.h"

#include <algorithm>

namespace tincan {

ForwardingDecision ForwardingPolicy::evaluate(const ForwardingSettings &settings, bool isBusy) {
    if (settings.dnd) {
        return {ForwardingAction::Reject486, {}, 0};
    }

    const QString alwaysUri = settings.always.trimmed();
    if (!alwaysUri.isEmpty()) {
        return {ForwardingAction::Redirect, alwaysUri, 0};
    }

    const QString busyUri = settings.onBusy.trimmed();
    if (isBusy && !busyUri.isEmpty()) {
        return {ForwardingAction::Redirect, busyUri, 0};
    }

    const QString noAnswerUri = settings.onNoAnswer.trimmed();
    if (!noAnswerUri.isEmpty()) {
        return {ForwardingAction::RedirectAfter, noAnswerUri, std::max(0, settings.noAnswerSec)};
    }

    return {ForwardingAction::Ring, {}, 0};
}

} // namespace tincan
