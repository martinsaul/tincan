// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "app/call_tone_controller.h"

namespace tincan {

namespace {

bool isOutgoingRingback(const CallInfo &call) {
    return call.direction == CallDirection::Outgoing && call.state == CallState::Early &&
           (call.lastStatusCode == 0 || call.lastStatusCode == 180);
}

} // namespace

CallToneController::CallToneController(ISipEngine &engine, SettingsStore &settings)
    : engine_(engine), settings_(settings) {}

void CallToneController::update(const QList<CallInfo> &calls) {
    bool anyRingingIncoming = false;
    bool anyOutgoingRingback = false;
    bool anyConfirmed = false;
    for (const CallInfo &c : calls) {
        anyRingingIncoming = anyRingingIncoming || c.isIncomingRinging();
        anyOutgoingRingback = anyOutgoingRingback || isOutgoingRingback(c);
        anyConfirmed = anyConfirmed || (c.state == CallState::Confirmed);
    }

    std::optional<Tone> desiredTone;
    QString desiredWav;
    if (anyRingingIncoming) {
        if (calls.size() > 1) {
            desiredTone = Tone::CallWaiting;
        } else {
            desiredTone = Tone::Ringing;
            desiredWav = settings_.generalSettings().ringtonePath;
        }
    } else if (anyOutgoingRingback && !anyConfirmed) {
        desiredTone = Tone::Ringback;
    }

    if (!desiredTone.has_value()) {
        if (activeTone_.has_value()) {
            engine_.stopTone();
            activeTone_.reset();
            activeToneWav_.clear();
        }
        return;
    }
    if (activeTone_ != desiredTone || activeToneWav_ != desiredWav) {
        engine_.startTone(*desiredTone, desiredWav);
        activeTone_ = desiredTone;
        activeToneWav_ = desiredWav;
    }
}

} // namespace tincan
