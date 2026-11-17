// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "app/audio_service.h"

#include <QSet>

#include <algorithm>

namespace tincan {

namespace {

int findDeviceIndexByName(const QList<AudioDevice> &devices, const QString &name, bool wantInput,
                          bool wantOutput) {
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty()) {
        return -1;
    }
    for (const AudioDevice &dev : devices) {
        const bool dirOk = (!wantInput || dev.inputs > 0) && (!wantOutput || dev.outputs > 0);
        if (dirOk && dev.name == trimmed) {
            return dev.index;
        }
    }
    for (const AudioDevice &dev : devices) {
        if (dev.name == trimmed) {
            return dev.index;
        }
    }
    return -1;
}

} // namespace

AudioService::AudioService(ISipEngine &engine, SettingsStore &settings, QObject *parent)
    : QObject(parent), engine_(engine), settings_(settings) {}

void AudioService::applyCurrentSettings() {
    applyToEngine(settings_.audioSettings());
}

void AudioService::setAudioSettings(const AudioSettings &settings) {
    settings_.setAudioSettings(settings);
    applyToEngine(settings);
    emit audioSettingsChanged(settings);
}

AudioSettings AudioService::audioSettings() const {
    return settings_.audioSettings();
}

QList<AudioDevice> AudioService::availableDevices() const {
    return engine_.audioDevices();
}

QList<CodecInfo> AudioService::availableCodecs() const {
    return engine_.codecs();
}

void AudioService::startRingtonePreview(const QString &wavPath) {
    engine_.startTone(Tone::Ringing, wavPath);
}

void AudioService::stopRingtonePreview() {
    engine_.stopTone();
}

void AudioService::applyToEngine(const AudioSettings &settings) {
    const QList<AudioDevice> devices = engine_.audioDevices();
    const int captureIdx = findDeviceIndexByName(devices, settings.captureName, true, false);
    const int playbackIdx = findDeviceIndexByName(devices, settings.playbackName, false, true);
    const int ringIdx = findDeviceIndexByName(devices, settings.ringName, false, true);

    (void)engine_.setAudioDevices(captureIdx, playbackIdx);
    engine_.setRingDevice(ringIdx);
    (void)engine_.setEchoCanceller(settings.echoCancel, settings.ecTailMs);

    if (settings.codecOrder.isEmpty() && settings.disabledCodecs.isEmpty()) {
        return;
    }

    const QList<CodecInfo> engineCodecs = engine_.codecs();
    QSet<QString> knownIds;
    for (const CodecInfo &c : engineCodecs) {
        knownIds.insert(c.id);
    }

    QStringList ordered;
    for (const QString &id : settings.codecOrder) {
        if (knownIds.contains(id) && !ordered.contains(id)) {
            ordered.append(id);
        }
    }
    for (const CodecInfo &c : engineCodecs) {
        if (!ordered.contains(c.id)) {
            ordered.append(c.id);
        }
    }

    const QSet<QString> disabled(settings.disabledCodecs.begin(), settings.disabledCodecs.end());
    for (qsizetype i = 0; i < ordered.size(); ++i) {
        const QString &id = ordered[i];
        const int prio = disabled.contains(id) ? 0 : std::clamp(254 - static_cast<int>(i), 1, 254);
        (void)engine_.setCodecPriority(id, prio);
    }
}

} // namespace tincan
