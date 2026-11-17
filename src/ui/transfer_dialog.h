// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include "core/call_info.h"

#include <QDialog>
#include <QList>
#include <QString>

class QComboBox;
class QLineEdit;
class QRadioButton;

namespace tincan::ui {

class TransferDialog : public QDialog {
    Q_OBJECT

public:
    explicit TransferDialog(const QList<CallInfo> &otherCalls, QWidget *parent = nullptr);

    bool isAttended() const;
    QString blindTarget() const;
    int attendedTargetCallId() const;

    void accept() override;

private:
    void syncModeEnabled();

    QRadioButton *blindRadio_ = nullptr;
    QLineEdit *destinationEdit_ = nullptr;
    QRadioButton *attendedRadio_ = nullptr;
    QComboBox *targetCombo_ = nullptr;
};

} // namespace tincan::ui
