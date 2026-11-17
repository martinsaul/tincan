// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "ui/transfer_dialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QRadioButton>
#include <QVBoxLayout>

namespace tincan::ui {

namespace {

QString describeCallTarget(const CallInfo &call) {
    const QString name = call.remoteDisplayName.trimmed();
    const QString uri = call.remoteUri.trimmed();
    if (!name.isEmpty() && !uri.isEmpty()) {
        return QStringLiteral("%1 (%2)").arg(name, uri);
    }
    return name.isEmpty() ? uri : name;
}

} // namespace

TransferDialog::TransferDialog(const QList<CallInfo> &otherCalls, QWidget *parent)
    : QDialog(parent) {
    setObjectName(QStringLiteral("transferDialog"));
    setWindowTitle(tr("Transfer Call"));
    setMinimumWidth(320);

    auto *root = new QVBoxLayout(this);
    root->setSpacing(8);

    blindRadio_ = new QRadioButton(tr("Blind transfer to number or SIP URI:"), this);
    blindRadio_->setObjectName(QStringLiteral("blindTransferRadio"));
    blindRadio_->setChecked(true);

    destinationEdit_ = new QLineEdit(this);
    destinationEdit_->setObjectName(QStringLiteral("transferDestinationEdit"));
    destinationEdit_->setPlaceholderText(tr("Number or SIP URI"));

    attendedRadio_ = new QRadioButton(tr("Attended transfer to another call:"), this);
    attendedRadio_->setObjectName(QStringLiteral("attendedTransferRadio"));

    targetCombo_ = new QComboBox(this);
    targetCombo_->setObjectName(QStringLiteral("transferTargetCombo"));
    if (otherCalls.isEmpty()) {
        targetCombo_->addItem(tr("No other active call"), -1);
        attendedRadio_->setEnabled(false);
        targetCombo_->setEnabled(false);
    } else {
        for (const CallInfo &c : otherCalls) {
            targetCombo_->addItem(describeCallTarget(c), c.callId);
        }
    }

    connect(blindRadio_, &QRadioButton::toggled, this,
            [this](bool) { syncModeEnabled(); });
    connect(attendedRadio_, &QRadioButton::toggled, this,
            [this](bool) { syncModeEnabled(); });

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &TransferDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &TransferDialog::reject);

    root->addWidget(blindRadio_);
    root->addWidget(destinationEdit_);
    root->addWidget(attendedRadio_);
    root->addWidget(targetCombo_);
    root->addStretch();
    root->addWidget(buttons);

    syncModeEnabled();
}

bool TransferDialog::isAttended() const {
    return attendedRadio_->isChecked();
}

QString TransferDialog::blindTarget() const {
    return destinationEdit_->text().trimmed();
}

int TransferDialog::attendedTargetCallId() const {
    return targetCombo_->currentData().toInt();
}

void TransferDialog::accept() {
    if (isAttended()) {
        if (attendedTargetCallId() < 0) {
            return;
        }
    } else if (blindTarget().isEmpty()) {
        destinationEdit_->setFocus();
        return;
    }
    QDialog::accept();
}

void TransferDialog::syncModeEnabled() {
    const bool attended = attendedRadio_->isChecked();
    destinationEdit_->setEnabled(!attended);
    if (attendedRadio_->isEnabled()) {
        targetCombo_->setEnabled(attended);
    }
}

} // namespace tincan::ui
