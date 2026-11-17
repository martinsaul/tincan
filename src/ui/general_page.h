// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include "app/audio_service.h"
#include "app/settings_store.h"

#include <QWidget>

class QCheckBox;
class QGroupBox;
class QLineEdit;
class QPushButton;

namespace tincan::ui {

class GeneralPage : public QWidget {
    Q_OBJECT

public:
    GeneralPage(SettingsStore &settingsStore, AudioService &audioService,
                QWidget *parent = nullptr);

    void applyChanges();
    void stopPreview();

private:
    QGroupBox *buildDesktopGroup();
    QGroupBox *buildRingtoneGroup();
    void browseRingtone();
    void togglePreview();

    SettingsStore &settingsStore_;
    AudioService &audioService_;
    bool previewPlaying_ = false;
    QCheckBox *startMinimizedCheck_ = nullptr;
    QCheckBox *closeToTrayCheck_ = nullptr;
    QCheckBox *autostartCheck_ = nullptr;
    QLineEdit *ringtoneEdit_ = nullptr;
    QPushButton *browseButton_ = nullptr;
    QPushButton *testButton_ = nullptr;
};

} // namespace tincan::ui
