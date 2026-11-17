// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include "core/call_info.h"

#include <QDateTime>
#include <QFrame>

class QLabel;
class QPushButton;
class QTimer;

namespace tincan::ui {

class CallCard : public QFrame {
    Q_OBJECT

public:
    explicit CallCard(const CallInfo &info, QWidget *parent = nullptr);

    int callId() const;
    void updateCall(const CallInfo &info);

    QPushButton *answerButton() const;
    QPushButton *hangupButton() const;

signals:
    void answerClicked(int callId);
    void hangupClicked(int callId);

private:
    void refreshDisplay();
    void refreshTimerText();
    QString stateLabelText() const;

    CallInfo info_;
    QDateTime localConfirmStart_;
    QLabel *remoteLabel_ = nullptr;
    QLabel *stateLabel_ = nullptr;
    QPushButton *answerButton_ = nullptr;
    QPushButton *hangupButton_ = nullptr;
    QTimer *durationTimer_ = nullptr;
};

} // namespace tincan::ui
