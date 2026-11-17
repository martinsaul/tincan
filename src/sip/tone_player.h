// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include "core/tone.h"

#include <QString>

#include <pjsua2/media.hpp>

#include <memory>

namespace tincan::sip {

class SipEndpoint;

// Plays call progress and alerting tones (or a custom 16-bit mono PCM WAV)
// through the conference bridge. Ring device index is stored via setRingDevice;
// playback reuses the active conference playback device so ALSA/PipeWire
// auto-close works without opening a second non-auto-closing ExtraAudioDevice,
// and so tone playback remains safe under nullAudio.
class TonePlayer {
public:
    explicit TonePlayer(SipEndpoint &endpoint);
    ~TonePlayer();

    TonePlayer(const TonePlayer &) = delete;
    TonePlayer &operator=(const TonePlayer &) = delete;

    void setRingDevice(int playbackIndex);
    int ringDevice() const;

    void startTone(Tone tone, const QString &customWav = {});
    void stopTone();

private:
    bool tryStartWav(const QString &wavPath, const pj::AudioMedia &sink);

    SipEndpoint &endpoint_;
    int ringDeviceIndex_ = -1;
    std::unique_ptr<pj::ToneGenerator> toneGen_;
    std::unique_ptr<pj::AudioMediaPlayer> wavPlayer_;
};

} // namespace tincan::sip
