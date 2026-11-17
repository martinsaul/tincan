// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "ui/calls_page.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace tincan::ui {

CallsPage::CallsPage(SettingsStore &settingsStore, QWidget *parent)
    : QWidget(parent), settingsStore_(settingsStore) {
    setObjectName(QStringLiteral("callsPage"));
    auto *root = new QVBoxLayout(this);
    root->addWidget(buildDialingAndAnswerGroup());
    root->addWidget(buildForwardingGroup());
    root->addWidget(buildRecordingGroup());
    root->addStretch();
    loadFromSettings();
}

QGroupBox *CallsPage::buildDialingAndAnswerGroup() {
    auto *group = new QGroupBox(tr("DTMF && Auto-Answer"), this);
    auto *form = new QFormLayout(group);

    dtmfCombo_ = new QComboBox(group);
    dtmfCombo_->setObjectName(QStringLiteral("dtmfMethodCombo"));
    dtmfCombo_->addItem(tr("RFC 4733 (RTP event)"), static_cast<int>(DtmfMethod::Rfc4733));
    dtmfCombo_->addItem(tr("SIP INFO"), static_cast<int>(DtmfMethod::SipInfo));
    dtmfCombo_->addItem(tr("In-band audio"), static_cast<int>(DtmfMethod::InBand));

    autoAnswerCombo_ = new QComboBox(group);
    autoAnswerCombo_->setObjectName(QStringLiteral("autoAnswerModeCombo"));
    autoAnswerCombo_->addItem(tr("Off"), static_cast<int>(AutoAnswerMode::Off));
    autoAnswerCombo_->addItem(tr("Always"), static_cast<int>(AutoAnswerMode::Always));
    autoAnswerCombo_->addItem(tr("Call-Info / Intercom header only"),
                              static_cast<int>(AutoAnswerMode::HeaderOnly));

    autoAnswerDelaySpin_ = new QSpinBox(group);
    autoAnswerDelaySpin_->setObjectName(QStringLiteral("autoAnswerDelaySpin"));
    autoAnswerDelaySpin_->setRange(0, 60);
    autoAnswerDelaySpin_->setSuffix(tr(" s"));

    form->addRow(tr("DTMF method:"), dtmfCombo_);
    form->addRow(tr("Auto-answer:"), autoAnswerCombo_);
    form->addRow(tr("Auto-answer delay:"), autoAnswerDelaySpin_);
    return group;
}

QGroupBox *CallsPage::buildForwardingGroup() {
    auto *group = new QGroupBox(tr("Do Not Disturb && Forwarding"), this);
    auto *vbox = new QVBoxLayout(group);

    dndCheck_ = new QCheckBox(tr("Do Not Disturb (reject incoming calls with 486 Busy)"), group);
    dndCheck_->setObjectName(QStringLiteral("dndCheck"));
    vbox->addWidget(dndCheck_);

    auto *form = new QFormLayout();
    forwardAlwaysEdit_ = new QLineEdit(group);
    forwardAlwaysEdit_->setObjectName(QStringLiteral("forwardAlwaysEdit"));
    forwardAlwaysEdit_->setPlaceholderText(tr("Number or SIP URI"));

    forwardBusyEdit_ = new QLineEdit(group);
    forwardBusyEdit_->setObjectName(QStringLiteral("forwardBusyEdit"));
    forwardBusyEdit_->setPlaceholderText(tr("Number or SIP URI"));

    auto *noAnsRow = new QHBoxLayout();
    forwardNoAnswerEdit_ = new QLineEdit(group);
    forwardNoAnswerEdit_->setObjectName(QStringLiteral("forwardNoAnswerEdit"));
    forwardNoAnswerEdit_->setPlaceholderText(tr("Number or SIP URI"));
    forwardNoAnswerSpin_ = new QSpinBox(group);
    forwardNoAnswerSpin_->setObjectName(QStringLiteral("forwardNoAnswerSpin"));
    forwardNoAnswerSpin_->setRange(1, 120);
    forwardNoAnswerSpin_->setSuffix(tr(" s"));
    noAnsRow->addWidget(forwardNoAnswerEdit_, 1);
    noAnsRow->addWidget(forwardNoAnswerSpin_);

    form->addRow(tr("Forward always:"), forwardAlwaysEdit_);
    form->addRow(tr("Forward when busy:"), forwardBusyEdit_);
    form->addRow(tr("Forward on no answer:"), noAnsRow);
    vbox->addLayout(form);
    return group;
}

QGroupBox *CallsPage::buildRecordingGroup() {
    auto *group = new QGroupBox(tr("Call Recording"), this);
    auto *vbox = new QVBoxLayout(group);

    auto *dirRow = new QHBoxLayout();
    recordingsDirEdit_ = new QLineEdit(group);
    recordingsDirEdit_->setObjectName(QStringLiteral("recordingsDirEdit"));
    browseRecordingsButton_ = new QPushButton(tr("Browse..."), group);
    browseRecordingsButton_->setObjectName(QStringLiteral("browseRecordingsButton"));
    connect(browseRecordingsButton_, &QPushButton::clicked, this, &CallsPage::browseRecordingsDir);
    dirRow->addWidget(recordingsDirEdit_, 1);
    dirRow->addWidget(browseRecordingsButton_);

    autoRecordCheck_ = new QCheckBox(tr("Automatically record all calls"), group);
    autoRecordCheck_->setObjectName(QStringLiteral("autoRecordCheck"));

    auto *form = new QFormLayout();
    form->addRow(tr("Recordings folder:"), dirRow);
    vbox->addLayout(form);
    vbox->addWidget(autoRecordCheck_);
    return group;
}

void CallsPage::loadFromSettings() {
    const CallSettings current = settingsStore_.callSettings();
    const int dtmfIdx = dtmfCombo_->findData(static_cast<int>(current.dtmfMethod));
    dtmfCombo_->setCurrentIndex(dtmfIdx >= 0 ? dtmfIdx : 0);

    const int aaIdx = autoAnswerCombo_->findData(static_cast<int>(current.autoAnswerMode));
    autoAnswerCombo_->setCurrentIndex(aaIdx >= 0 ? aaIdx : 0);

    autoAnswerDelaySpin_->setValue(current.autoAnswerDelaySec);
    dndCheck_->setChecked(current.dnd);
    forwardAlwaysEdit_->setText(current.forwardAlways);
    forwardBusyEdit_->setText(current.forwardOnBusy);
    forwardNoAnswerEdit_->setText(current.forwardOnNoAnswer);
    forwardNoAnswerSpin_->setValue(current.forwardNoAnswerSec > 0 ? current.forwardNoAnswerSec : 15);
    recordingsDirEdit_->setText(current.recordingsDir);
    autoRecordCheck_->setChecked(current.autoRecord);
}

void CallsPage::applyChanges() {
    CallSettings updated;
    updated.dtmfMethod = static_cast<DtmfMethod>(dtmfCombo_->currentData().toInt());
    updated.autoAnswerMode = static_cast<AutoAnswerMode>(autoAnswerCombo_->currentData().toInt());
    updated.autoAnswerDelaySec = autoAnswerDelaySpin_->value();
    updated.dnd = dndCheck_->isChecked();
    updated.forwardAlways = forwardAlwaysEdit_->text().trimmed();
    updated.forwardOnBusy = forwardBusyEdit_->text().trimmed();
    updated.forwardOnNoAnswer = forwardNoAnswerEdit_->text().trimmed();
    updated.forwardNoAnswerSec = forwardNoAnswerSpin_->value();
    updated.recordingsDir = recordingsDirEdit_->text().trimmed();
    updated.autoRecord = autoRecordCheck_->isChecked();
    settingsStore_.setCallSettings(updated);
}

void CallsPage::browseRecordingsDir() {
    const QString selected = QFileDialog::getExistingDirectory(
        this, tr("Select Recordings Directory"), recordingsDirEdit_->text());
    if (!selected.isEmpty()) {
        recordingsDirEdit_->setText(selected);
    }
}

} // namespace tincan::ui
