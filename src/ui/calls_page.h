// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include "app/settings_store.h"

#include <QWidget>

class QCheckBox;
class QComboBox;
class QGroupBox;
class QLineEdit;
class QPushButton;
class QSpinBox;

namespace tincan::ui {

class CallsPage : public QWidget {
    Q_OBJECT

public:
    explicit CallsPage(SettingsStore &settingsStore, QWidget *parent = nullptr);

    void applyChanges();

private:
    QGroupBox *buildDialingAndAnswerGroup();
    QGroupBox *buildForwardingGroup();
    QGroupBox *buildRecordingGroup();
    void loadFromSettings();
    void browseRecordingsDir();

    SettingsStore &settingsStore_;
    QComboBox *dtmfCombo_ = nullptr;
    QCheckBox *keypadTonesCheck_ = nullptr;
    QComboBox *autoAnswerCombo_ = nullptr;
    QSpinBox *autoAnswerDelaySpin_ = nullptr;
    QCheckBox *dndCheck_ = nullptr;
    QLineEdit *forwardAlwaysEdit_ = nullptr;
    QLineEdit *forwardBusyEdit_ = nullptr;
    QLineEdit *forwardNoAnswerEdit_ = nullptr;
    QSpinBox *forwardNoAnswerSpin_ = nullptr;
    QLineEdit *recordingsDirEdit_ = nullptr;
    QPushButton *browseRecordingsButton_ = nullptr;
    QCheckBox *autoRecordCheck_ = nullptr;
};

} // namespace tincan::ui
