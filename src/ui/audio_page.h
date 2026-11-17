// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include "app/audio_service.h"

#include <QWidget>

class QCheckBox;
class QComboBox;
class QFormLayout;
class QGroupBox;
class QListWidget;
class QPushButton;
class QSpinBox;

namespace tincan::ui {

class AudioPage : public QWidget {
    Q_OBJECT

public:
    explicit AudioPage(AudioService &audioService, QWidget *parent = nullptr);

    void applyChanges();

private:
    QFormLayout *buildDeviceForm();
    QGroupBox *buildCodecGroup();
    void populateDevices(const AudioSettings &current);
    void populateCodecs(const AudioSettings &current);
    void moveSelectedCodec(int delta);
    void updateMoveButtons();

    AudioService &audioService_;
    QComboBox *captureCombo_ = nullptr;
    QComboBox *playbackCombo_ = nullptr;
    QComboBox *ringCombo_ = nullptr;
    QCheckBox *echoCancelCheck_ = nullptr;
    QSpinBox *ecTailSpin_ = nullptr;
    QListWidget *codecList_ = nullptr;
    QPushButton *moveUpButton_ = nullptr;
    QPushButton *moveDownButton_ = nullptr;
};

} // namespace tincan::ui
