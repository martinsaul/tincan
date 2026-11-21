// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include <QChar>
#include <QTimer>

#include <pjsua2/media.hpp>

#include <memory>

namespace tincan::sip {

class SipEndpoint;

// Local keypad feedback: one DTMF dual-tone burst per key press.
//
// A single ToneGenerator is created on the first press and connected to the
// playback side of the sound device port (conference slot 0) only. Nothing
// transmits from it to a call or a recorder, and slot 0's own transmissions
// carry the microphone, not what is played to it, so the tone stays on the
// speaker. Further presses restart that generator instead of adding ports;
// after a short idle period it is disconnected and destroyed.
// Main thread only, like every other pjsua media operation in Tincan.
class KeypadTonePlayer {
public:
    explicit KeypadTonePlayer(SipEndpoint &endpoint);
    ~KeypadTonePlayer();

    KeypadTonePlayer(const KeypadTonePlayer &) = delete;
    KeypadTonePlayer &operator=(const KeypadTonePlayer &) = delete;

    void play(QChar digit);
    void release();

    // Conference slot of the generator, or -1 while none exists.
    int portId() const;

private:
    bool ensureGenerator();

    SipEndpoint &endpoint_;
    std::unique_ptr<pj::ToneGenerator> generator_;
    QTimer idleTimer_;
};

} // namespace tincan::sip
