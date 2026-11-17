// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "sip/tone_player.h"

#include "sip/sip_endpoint.h"

namespace tincan::sip {

namespace {

pj::ToneDesc makeToneDesc(short freq1, short freq2, short onMs, short offMs, short volume) {
    pj::ToneDesc desc;
    desc.freq1 = freq1;
    desc.freq2 = freq2;
    desc.on_msec = onMs;
    desc.off_msec = offMs;
    desc.volume = volume;
    desc.flags = 0;
    return desc;
}

pj::ToneDescVector buildToneDescriptors(Tone tone) {
    pj::ToneDescVector tones;
    switch (tone) {
    case Tone::Ringing:
        tones.push_back(makeToneDesc(440, 480, 2000, 4000, 12000));
        break;
    case Tone::Ringback:
        tones.push_back(makeToneDesc(440, 480, 2000, 4000, 5000));
        break;
    case Tone::CallWaiting:
        tones.push_back(makeToneDesc(440, 0, 300, 200, 6000));
        tones.push_back(makeToneDesc(440, 0, 300, 9200, 6000));
        break;
    }
    return tones;
}

} // namespace

TonePlayer::TonePlayer(SipEndpoint &endpoint) : endpoint_(endpoint) {}

TonePlayer::~TonePlayer() {
    stopTone();
}

void TonePlayer::setRingDevice(int playbackIndex) {
    ringDeviceIndex_ = playbackIndex;
}

int TonePlayer::ringDevice() const {
    return ringDeviceIndex_;
}

void TonePlayer::startTone(Tone tone, const QString &customWav) {
    stopTone();
    if (!endpoint_.isStarted()) {
        return;
    }

    try {
        pj::AudioMedia &sink = endpoint_.pjEndpoint().audDevManager().getPlaybackDevMedia();
        const QString wav = customWav.trimmed();
        if (!wav.isEmpty() && tryStartWav(wav, sink)) {
            return;
        }

        toneGen_ = std::make_unique<pj::ToneGenerator>();
        toneGen_->createToneGenerator();
        toneGen_->play(buildToneDescriptors(tone), true);
        toneGen_->startTransmit(sink);
    } catch (const pj::Error &) {
        stopTone();
    }
}

bool TonePlayer::tryStartWav(const QString &wavPath, const pj::AudioMedia &sink) {
    try {
        wavPlayer_ = std::make_unique<pj::AudioMediaPlayer>();
        wavPlayer_->createPlayer(wavPath.toStdString(), 0);
        wavPlayer_->startTransmit(sink);
        return true;
    } catch (const pj::Error &) {
        wavPlayer_.reset();
        return false;
    }
}

void TonePlayer::stopTone() {
    if (wavPlayer_) {
        if (endpoint_.isStarted()) {
            try {
                wavPlayer_->stopTransmit(
                    endpoint_.pjEndpoint().audDevManager().getPlaybackDevMedia());
            } catch (const pj::Error &) {
            }
        }
        wavPlayer_.reset();
    }

    if (toneGen_) {
        try {
            toneGen_->stop();
        } catch (const pj::Error &) {
        }
        if (endpoint_.isStarted()) {
            try {
                toneGen_->stopTransmit(
                    endpoint_.pjEndpoint().audDevManager().getPlaybackDevMedia());
            } catch (const pj::Error &) {
            }
        }
        toneGen_.reset();
    }
}

} // namespace tincan::sip
