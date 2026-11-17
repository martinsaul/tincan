// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "ui/general_page.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace tincan::ui {

GeneralPage::GeneralPage(SettingsStore &settingsStore, AudioService &audioService, QWidget *parent)
    : QWidget(parent), settingsStore_(settingsStore), audioService_(audioService) {
    setObjectName(QStringLiteral("generalPage"));
    auto *root = new QVBoxLayout(this);
    root->addWidget(buildDesktopGroup());
    root->addWidget(buildAlertGroup());
    root->addWidget(buildRingtoneGroup());
    root->addStretch();

    const GeneralSettings current = settingsStore_.generalSettings();
    startMinimizedCheck_->setChecked(current.startMinimized);
    closeToTrayCheck_->setChecked(current.closeToTray);
    autostartCheck_->setChecked(current.autostart);
    ringtoneEdit_->setText(current.ringtonePath);
    incomingAlertCombo_->setCurrentIndex(
        incomingAlertCombo_->findData(static_cast<int>(current.incomingAlert)));
}

QGroupBox *GeneralPage::buildAlertGroup() {
    auto *group = new QGroupBox(tr("Notifications"), this);
    auto *layout = new QVBoxLayout(group);
    auto *label = new QLabel(tr("Announce incoming calls with:"), group);
    incomingAlertCombo_ = new QComboBox(group);
    incomingAlertCombo_->setObjectName(QStringLiteral("incomingAlertCombo"));
    incomingAlertCombo_->addItem(tr("Popup window"), static_cast<int>(IncomingAlertMode::Popup));
    incomingAlertCombo_->addItem(tr("Desktop notification"),
                                 static_cast<int>(IncomingAlertMode::Notification));
    incomingAlertCombo_->addItem(tr("Popup and desktop notification"),
                                 static_cast<int>(IncomingAlertMode::Both));
    label->setBuddy(incomingAlertCombo_);
    layout->addWidget(label);
    layout->addWidget(incomingAlertCombo_);
    return group;
}

QGroupBox *GeneralPage::buildDesktopGroup() {
    auto *desktopGroup = new QGroupBox(tr("Desktop Behavior"), this);
    auto *desktopLayout = new QVBoxLayout(desktopGroup);

    startMinimizedCheck_ = new QCheckBox(tr("Start minimized"), desktopGroup);
    startMinimizedCheck_->setObjectName(QStringLiteral("startMinimizedCheck"));
    closeToTrayCheck_ = new QCheckBox(tr("Close window to system tray"), desktopGroup);
    closeToTrayCheck_->setObjectName(QStringLiteral("closeToTrayCheck"));
    autostartCheck_ = new QCheckBox(tr("Start automatically on login"), desktopGroup);
    autostartCheck_->setObjectName(QStringLiteral("autostartCheck"));

    desktopLayout->addWidget(startMinimizedCheck_);
    desktopLayout->addWidget(closeToTrayCheck_);
    desktopLayout->addWidget(autostartCheck_);
    return desktopGroup;
}

QGroupBox *GeneralPage::buildRingtoneGroup() {
    auto *ringGroup = new QGroupBox(tr("Ringtone"), this);
    auto *ringLayout = new QVBoxLayout(ringGroup);
    auto *hintLabel =
        new QLabel(tr("Custom WAV file (leave empty for built-in ringtone):"), ringGroup);
    hintLabel->setWordWrap(true);

    auto *fileRow = new QHBoxLayout();
    ringtoneEdit_ = new QLineEdit(ringGroup);
    ringtoneEdit_->setObjectName(QStringLiteral("ringtoneEdit"));
    ringtoneEdit_->setPlaceholderText(tr("Built-in 440/480 Hz ringtone"));

    browseButton_ = new QPushButton(tr("Browse..."), ringGroup);
    browseButton_->setObjectName(QStringLiteral("browseRingtoneButton"));
    connect(browseButton_, &QPushButton::clicked, this, &GeneralPage::browseRingtone);

    testButton_ = new QPushButton(tr("Test"), ringGroup);
    testButton_->setObjectName(QStringLiteral("testRingtoneButton"));
    connect(testButton_, &QPushButton::clicked, this, &GeneralPage::togglePreview);

    fileRow->addWidget(ringtoneEdit_, 1);
    fileRow->addWidget(browseButton_);
    fileRow->addWidget(testButton_);

    ringLayout->addWidget(hintLabel);
    ringLayout->addLayout(fileRow);
    return ringGroup;
}

void GeneralPage::applyChanges() {
    GeneralSettings updated;
    updated.startMinimized = startMinimizedCheck_->isChecked();
    updated.closeToTray = closeToTrayCheck_->isChecked();
    updated.autostart = autostartCheck_->isChecked();
    updated.ringtonePath = ringtoneEdit_->text().trimmed();
    updated.incomingAlert =
        static_cast<IncomingAlertMode>(incomingAlertCombo_->currentData().toInt());
    settingsStore_.setGeneralSettings(updated);
}

void GeneralPage::stopPreview() {
    if (previewPlaying_) {
        audioService_.stopRingtonePreview();
        previewPlaying_ = false;
        testButton_->setText(tr("Test"));
    }
}

void GeneralPage::browseRingtone() {
    const QString selected = QFileDialog::getOpenFileName(
        this, tr("Select Ringtone WAV"), ringtoneEdit_->text(), tr("WAV Audio (*.wav)"));
    if (!selected.isEmpty()) {
        ringtoneEdit_->setText(selected);
    }
}

void GeneralPage::togglePreview() {
    if (previewPlaying_) {
        stopPreview();
        return;
    }
    audioService_.startRingtonePreview(ringtoneEdit_->text().trimmed());
    previewPlaying_ = true;
    testButton_->setText(tr("Stop"));
}

} // namespace tincan::ui
