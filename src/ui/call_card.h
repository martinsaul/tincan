// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include "core/call_info.h"

#include <QDateTime>
#include <QFrame>
#include <QString>

class QLabel;
class QMouseEvent;
class QPushButton;
class QSlider;
class QTimer;
class QWidget;

namespace tincan::ui {

class DtmfPad;

class CallCard : public QFrame {
    Q_OBJECT

public:
    explicit CallCard(const CallInfo &info, QWidget *parent = nullptr);

    int callId() const;
    const CallInfo &callInfo() const;
    void updateCall(const CallInfo &info);
    void setActiveHighlight(bool active);
    void setKeypadExpanded(bool expanded);
    void appendDtmfEcho(const QString &digits);
    QString dtmfEcho() const;

    QPushButton *answerButton() const;
    QPushButton *hangupButton() const;
    QPushButton *holdButton() const;
    QPushButton *muteButton() const;
    QPushButton *keypadButton() const;
    QPushButton *transferButton() const;
    QPushButton *recordButton() const;
    QSlider *volumeSlider() const;
    DtmfPad *dtmfPad() const;

signals:
    void answerClicked(int callId);
    void hangupClicked(int callId);
    void holdToggled(int callId, bool hold);
    void muteToggled(int callId, bool mute);
    void dtmfDigitPressed(int callId, const QString &digit);
    void transferClicked(int callId);
    void recordToggled(int callId);
    void volumeChanged(int callId, float rxLevel);
    void activated(int callId);

protected:
    void mousePressEvent(QMouseEvent *event) override;

private:
    QWidget *buildInCallControls();
    QWidget *buildBottomActionRow();
    void refreshDisplay();
    void refreshControlStates();
    void refreshStateText();
    QString baseStateText() const;
    QString composeStatusLine() const;
    void updateCardStyle();

    CallInfo info_;
    bool activeHighlight_ = false;
    QString echoedDigits_;
    QDateTime localConfirmStart_;

    QLabel *remoteLabel_ = nullptr;
    QLabel *stateLabel_ = nullptr;
    QLabel *dtmfEchoLabel_ = nullptr;
    QWidget *inCallBox_ = nullptr;
    QPushButton *holdButton_ = nullptr;
    QPushButton *muteButton_ = nullptr;
    QPushButton *keypadButton_ = nullptr;
    QPushButton *transferButton_ = nullptr;
    DtmfPad *dtmfPad_ = nullptr;
    QPushButton *recordButton_ = nullptr;
    QWidget *volumeBox_ = nullptr;
    QSlider *volumeSlider_ = nullptr;
    QPushButton *answerButton_ = nullptr;
    QPushButton *hangupButton_ = nullptr;
    QTimer *durationTimer_ = nullptr;
};

} // namespace tincan::ui
