// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include "app/settings_store.h"
#include "core/audio_device.h"
#include "core/codec_info.h"
#include "core/sip_engine.h"

#include <QList>
#include <QObject>
#include <QString>

namespace tincan {

class AudioService : public QObject {
    Q_OBJECT

public:
    AudioService(ISipEngine &engine, SettingsStore &settings, QObject *parent = nullptr);

    void applyCurrentSettings();
    void setAudioSettings(const AudioSettings &settings);
    AudioSettings audioSettings() const;

    QList<AudioDevice> availableDevices() const;
    QList<CodecInfo> availableCodecs() const;

    void startRingtonePreview(const QString &wavPath);
    void stopRingtonePreview();

    // Local feedback for a pressed keypad key, when "Play keypad tones" is on.
    // Only the speaker hears it; DTMF to a call is sent separately.
    void playKeypadTone(const QString &digit);

signals:
    void audioSettingsChanged(const tincan::AudioSettings &settings);

private:
    void applyToEngine(const AudioSettings &settings);

    ISipEngine &engine_;
    SettingsStore &settings_;
};

} // namespace tincan
