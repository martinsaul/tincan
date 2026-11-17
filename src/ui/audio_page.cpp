// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "ui/audio_page.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QListWidget>
#include <QMap>
#include <QPushButton>
#include <QSet>
#include <QSpinBox>
#include <QVBoxLayout>

namespace tincan::ui {

namespace {

void selectOrInsertSavedDevice(QComboBox *combo, const QString &savedName) {
    if (savedName.trimmed().isEmpty()) {
        combo->setCurrentIndex(0);
        return;
    }
    const int idx = combo->findData(savedName);
    if (idx >= 0) {
        combo->setCurrentIndex(idx);
        return;
    }
    combo->addItem(QObject::tr("%1 (unavailable)").arg(savedName), savedName);
    combo->setCurrentIndex(combo->count() - 1);
}

} // namespace

AudioPage::AudioPage(AudioService &audioService, QWidget *parent)
    : QWidget(parent), audioService_(audioService) {
    setObjectName(QStringLiteral("audioPage"));
    auto *root = new QVBoxLayout(this);
    root->addLayout(buildDeviceForm());
    root->addWidget(buildCodecGroup(), 1);

    const AudioSettings current = audioService_.audioSettings();
    populateDevices(current);
    echoCancelCheck_->setChecked(current.echoCancel);
    ecTailSpin_->setValue(current.ecTailMs > 0 ? current.ecTailMs : 200);
    ecTailSpin_->setEnabled(current.echoCancel);
    populateCodecs(current);
}

QFormLayout *AudioPage::buildDeviceForm() {
    auto *devForm = new QFormLayout();
    devForm->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    captureCombo_ = new QComboBox(this);
    captureCombo_->setObjectName(QStringLiteral("captureCombo"));
    playbackCombo_ = new QComboBox(this);
    playbackCombo_->setObjectName(QStringLiteral("playbackCombo"));
    ringCombo_ = new QComboBox(this);
    ringCombo_->setObjectName(QStringLiteral("ringCombo"));

    echoCancelCheck_ = new QCheckBox(tr("Enable acoustic echo cancellation"), this);
    echoCancelCheck_->setObjectName(QStringLiteral("echoCancelCheck"));
    ecTailSpin_ = new QSpinBox(this);
    ecTailSpin_->setObjectName(QStringLiteral("ecTailSpin"));
    ecTailSpin_->setRange(10, 1000);
    ecTailSpin_->setSingleStep(10);
    ecTailSpin_->setSuffix(tr(" ms"));
    connect(echoCancelCheck_, &QCheckBox::toggled, ecTailSpin_, &QWidget::setEnabled);

    devForm->addRow(tr("Microphone:"), captureCombo_);
    devForm->addRow(tr("Playback:"), playbackCombo_);
    devForm->addRow(tr("Ringing device:"), ringCombo_);
    devForm->addRow(QString{}, echoCancelCheck_);
    devForm->addRow(tr("Echo tail length:"), ecTailSpin_);
    return devForm;
}

QGroupBox *AudioPage::buildCodecGroup() {
    auto *codecGroup = new QGroupBox(tr("Audio Codecs"), this);
    auto *codecLayout = new QHBoxLayout(codecGroup);
    codecList_ = new QListWidget(codecGroup);
    codecList_->setObjectName(QStringLiteral("codecListWidget"));
    connect(codecList_, &QListWidget::currentRowChanged, this,
            [this](int) { updateMoveButtons(); });

    auto *btnCol = new QVBoxLayout();
    moveUpButton_ = new QPushButton(tr("Up"), codecGroup);
    moveUpButton_->setObjectName(QStringLiteral("codecUpButton"));
    moveDownButton_ = new QPushButton(tr("Down"), codecGroup);
    moveDownButton_->setObjectName(QStringLiteral("codecDownButton"));
    connect(moveUpButton_, &QPushButton::clicked, this, [this]() { moveSelectedCodec(-1); });
    connect(moveDownButton_, &QPushButton::clicked, this, [this]() { moveSelectedCodec(1); });
    btnCol->addWidget(moveUpButton_);
    btnCol->addWidget(moveDownButton_);
    btnCol->addStretch();

    codecLayout->addWidget(codecList_, 1);
    codecLayout->addLayout(btnCol);
    return codecGroup;
}

void AudioPage::populateDevices(const AudioSettings &current) {
    captureCombo_->clear();
    playbackCombo_->clear();
    ringCombo_->clear();

    captureCombo_->addItem(tr("System Default"), QString{});
    playbackCombo_->addItem(tr("System Default"), QString{});
    ringCombo_->addItem(tr("System Default"), QString{});

    for (const AudioDevice &dev : audioService_.availableDevices()) {
        if (dev.inputs > 0) {
            captureCombo_->addItem(dev.name, dev.name);
        }
        if (dev.outputs > 0) {
            playbackCombo_->addItem(dev.name, dev.name);
            ringCombo_->addItem(dev.name, dev.name);
        }
    }

    selectOrInsertSavedDevice(captureCombo_, current.captureName);
    selectOrInsertSavedDevice(playbackCombo_, current.playbackName);
    selectOrInsertSavedDevice(ringCombo_, current.ringName);
}

void AudioPage::populateCodecs(const AudioSettings &current) {
    codecList_->clear();
    const QList<CodecInfo> engineCodecs = audioService_.availableCodecs();
    QMap<QString, int> prioById;
    for (const CodecInfo &c : engineCodecs) {
        prioById.insert(c.id, c.priority);
    }

    QStringList ordered;
    for (const QString &id : current.codecOrder) {
        if (prioById.contains(id) && !ordered.contains(id)) {
            ordered.append(id);
        }
    }
    for (const CodecInfo &c : engineCodecs) {
        if (!ordered.contains(c.id)) {
            ordered.append(c.id);
        }
    }

    const bool hasSavedPrefs = !current.codecOrder.isEmpty() || !current.disabledCodecs.isEmpty();
    const QSet<QString> disabled(current.disabledCodecs.begin(), current.disabledCodecs.end());

    for (const QString &id : ordered) {
        auto *item = new QListWidgetItem(id, codecList_);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        const bool enabled =
            hasSavedPrefs ? !disabled.contains(id) : (prioById.value(id, 1) > 0);
        item->setCheckState(enabled ? Qt::Checked : Qt::Unchecked);
    }
    if (codecList_->count() > 0) {
        codecList_->setCurrentRow(0);
    }
    updateMoveButtons();
}

void AudioPage::moveSelectedCodec(int delta) {
    const int row = codecList_->currentRow();
    const int target = row + delta;
    if (row < 0 || target < 0 || target >= codecList_->count()) {
        return;
    }
    QListWidgetItem *item = codecList_->takeItem(row);
    codecList_->insertItem(target, item);
    codecList_->setCurrentRow(target);
    updateMoveButtons();
}

void AudioPage::updateMoveButtons() {
    const int row = codecList_->currentRow();
    moveUpButton_->setEnabled(row > 0);
    moveDownButton_->setEnabled(row >= 0 && row + 1 < codecList_->count());
}

void AudioPage::applyChanges() {
    AudioSettings updated;
    updated.captureName = captureCombo_->currentData().toString();
    updated.playbackName = playbackCombo_->currentData().toString();
    updated.ringName = ringCombo_->currentData().toString();
    updated.echoCancel = echoCancelCheck_->isChecked();
    updated.ecTailMs = ecTailSpin_->value();

    for (int i = 0; i < codecList_->count(); ++i) {
        const QListWidgetItem *item = codecList_->item(i);
        updated.codecOrder.append(item->text());
        if (item->checkState() != Qt::Checked) {
            updated.disabledCodecs.append(item->text());
        }
    }

    audioService_.setAudioSettings(updated);
}

} // namespace tincan::ui
