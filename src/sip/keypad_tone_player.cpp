// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "sip/keypad_tone_player.h"

#include "core/dtmf_tone.h"
#include "sip/sip_endpoint.h"

namespace tincan::sip {

namespace {

// Same level as the ringback tone.
constexpr short kKeypadToneVolume = 5000;
// Keep the generator connected while the user is still typing.
constexpr int kIdleReleaseMs = 1000;

} // namespace

KeypadTonePlayer::KeypadTonePlayer(SipEndpoint &endpoint) : endpoint_(endpoint) {
    idleTimer_.setSingleShot(true);
    idleTimer_.setInterval(kIdleReleaseMs);
    QObject::connect(&idleTimer_, &QTimer::timeout, [this]() { release(); });
}

KeypadTonePlayer::~KeypadTonePlayer() {
    release();
}

void KeypadTonePlayer::play(QChar digit) {
    const std::optional<DtmfTonePair> pair = dtmfTonePair(digit);
    if (!pair.has_value() || !endpoint_.isStarted() || !ensureGenerator()) {
        return;
    }

    pj::ToneDesc desc;
    desc.freq1 = static_cast<short>(pair->lowHz);
    desc.freq2 = static_cast<short>(pair->highHz);
    desc.on_msec = static_cast<short>(kKeypadToneMs);
    desc.off_msec = 0;
    desc.volume = kKeypadToneVolume;
    desc.flags = 0;

    try {
        // Replace whatever burst is still sounding rather than queueing.
        generator_->stop();
        generator_->play(pj::ToneDescVector{desc}, false);
        idleTimer_.start();
    } catch (const pj::Error &) {
        release();
    }
}

bool KeypadTonePlayer::ensureGenerator() {
    if (generator_) {
        return true;
    }
    try {
        generator_ = std::make_unique<pj::ToneGenerator>();
        generator_->createToneGenerator();
        // The only connection this port ever gets: towards the speaker.
        generator_->startTransmit(endpoint_.pjEndpoint().audDevManager().getPlaybackDevMedia());
        return true;
    } catch (const pj::Error &) {
        release();
        return false;
    }
}

void KeypadTonePlayer::release() {
    idleTimer_.stop();
    if (!generator_) {
        return;
    }
    try {
        generator_->stop();
    } catch (const pj::Error &) {
    }
    if (endpoint_.isStarted()) {
        try {
            generator_->stopTransmit(endpoint_.pjEndpoint().audDevManager().getPlaybackDevMedia());
        } catch (const pj::Error &) {
        }
    }
    generator_.reset();
}

int KeypadTonePlayer::portId() const {
    return generator_ ? generator_->getPortId() : -1;
}

} // namespace tincan::sip
