// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "ui/call_card.h"

#include "ui/dtmf_pad.h"
#include "ui/icons.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QStringList>
#include <QTimer>
#include <QVBoxLayout>

#include <cmath>

namespace tincan::ui {

namespace {

QString formatRemoteParty(const CallInfo &info) {
    const QString name = info.remoteDisplayName.trimmed();
    const QString uri = info.remoteUri.trimmed();
    if (!name.isEmpty() && !uri.isEmpty()) {
        return QStringLiteral("%1 (%2)").arg(name, uri);
    }
    return name.isEmpty() ? uri : name;
}

QString formatDurationMmSs(qint64 seconds) {
    const qint64 clamped = seconds < 0 ? 0 : seconds;
    return QStringLiteral("%1:%2")
        .arg(clamped / 60, 2, 10, QLatin1Char('0'))
        .arg(clamped % 60, 2, 10, QLatin1Char('0'));
}

QPushButton *makePrimaryButton(const QIcon &icon, const QString &text, const QString &objName,
                               const QString &bgHex, QWidget *parent) {
    auto *btn = new QPushButton(icon, text, parent);
    btn->setObjectName(objName);
    btn->setStyleSheet(QStringLiteral("QPushButton { background-color: %1; color: #ffffff; "
                                      "font-weight: bold; border-radius: 4px; padding: 4px 8px; }")
                           .arg(bgHex));
    return btn;
}

QPushButton *makeSecondaryButton(const QString &text, const QString &objName, bool checkable,
                                 QWidget *parent) {
    auto *btn = new QPushButton(text, parent);
    btn->setObjectName(objName);
    btn->setCheckable(checkable);
    btn->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #e2e8f0; color: #0f172a; border: 1px solid #cbd5e1; "
        "border-radius: 4px; padding: 3px 6px; font-size: 11px; font-weight: 600; }"
        "QPushButton:checked { background-color: #2563eb; color: #ffffff; border-color: #1d4ed8; }"));
    return btn;
}

QString recordButtonStyle(bool recording) {
    if (recording) {
        return QStringLiteral(
            "QPushButton { background-color: #dc2626; color: #ffffff; border: 1px solid #b91c1c; "
            "border-radius: 4px; padding: 3px 6px; font-size: 11px; font-weight: bold; }");
    }
    return QStringLiteral(
        "QPushButton { background-color: #e2e8f0; color: #0f172a; border: 1px solid #cbd5e1; "
        "border-radius: 4px; padding: 3px 6px; font-size: 11px; font-weight: 600; }");
}

} // namespace

CallCard::CallCard(const CallInfo &info, QWidget *parent) : QFrame(parent), info_(info) {
    setObjectName(QStringLiteral("callCard"));
    setFrameShape(QFrame::StyledPanel);
    updateCardStyle();

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(5);

    remoteLabel_ = new QLabel(this);
    remoteLabel_->setObjectName(QStringLiteral("remoteLabel"));
    QFont remoteFont = remoteLabel_->font();
    remoteFont.setBold(true);
    remoteLabel_->setFont(remoteFont);
    remoteLabel_->setWordWrap(true);

    stateLabel_ = new QLabel(this);
    stateLabel_->setObjectName(QStringLiteral("stateLabel"));
    stateLabel_->setWordWrap(true);
    stateLabel_->setStyleSheet(QStringLiteral("color: #475569; font-size: 11px;"));

    dtmfEchoLabel_ = new QLabel(this);
    dtmfEchoLabel_->setObjectName(QStringLiteral("dtmfEchoLabel"));
    dtmfEchoLabel_->setStyleSheet(QStringLiteral(
        "background-color: #eef2ff; color: #1e3a8a; border-radius: 3px; padding: 2px 6px; "
        "font-family: monospace; font-size: 11px;"));
    dtmfEchoLabel_->setVisible(false);

    root->addWidget(remoteLabel_);
    root->addWidget(stateLabel_);
    root->addWidget(dtmfEchoLabel_);
    root->addWidget(buildInCallControls());
    root->addWidget(buildBottomActionRow());

    durationTimer_ = new QTimer(this);
    durationTimer_->setInterval(1000);
    connect(durationTimer_, &QTimer::timeout, this, &CallCard::refreshStateText);
    refreshDisplay();
}

QWidget *CallCard::buildInCallControls() {
    inCallBox_ = new QWidget(this);
    inCallBox_->setObjectName(QStringLiteral("inCallControls"));
    auto *vbox = new QVBoxLayout(inCallBox_);
    vbox->setContentsMargins(0, 0, 0, 0);
    vbox->setSpacing(4);

    auto *topRow = new QHBoxLayout();
    topRow->setSpacing(4);
    holdButton_ = makeSecondaryButton(tr("Hold"), QStringLiteral("holdButton"), true, inCallBox_);
    muteButton_ = makeSecondaryButton(tr("Mute"), QStringLiteral("muteButton"), true, inCallBox_);
    keypadButton_ =
        makeSecondaryButton(tr("Keypad"), QStringLiteral("keypadButton"), true, inCallBox_);
    transferButton_ =
        makeSecondaryButton(tr("Transfer"), QStringLiteral("transferButton"), false, inCallBox_);

    topRow->addWidget(holdButton_, 1);
    topRow->addWidget(muteButton_, 1);
    topRow->addWidget(keypadButton_, 1);
    topRow->addWidget(transferButton_, 1);

    dtmfPad_ = new DtmfPad(inCallBox_);
    dtmfPad_->setVisible(false);

    connect(holdButton_, &QPushButton::clicked, this,
            [this](bool checked) { emit holdToggled(info_.callId, checked); });
    connect(muteButton_, &QPushButton::clicked, this,
            [this](bool checked) { emit muteToggled(info_.callId, checked); });
    connect(keypadButton_, &QPushButton::toggled, this, [this](bool open) {
        dtmfPad_->setVisible(open);
        emit keypadToggled(info_.callId, open);
    });
    connect(transferButton_, &QPushButton::clicked, this,
            [this]() { emit transferClicked(info_.callId); });
    connect(dtmfPad_, &DtmfPad::digitPressed, this,
            [this](const QString &d) { emit dtmfDigitPressed(info_.callId, d); });

    vbox->addLayout(topRow);
    vbox->addWidget(dtmfPad_);
    return inCallBox_;
}

QWidget *CallCard::buildBottomActionRow() {
    auto *rowWidget = new QWidget(this);
    auto *row = new QHBoxLayout(rowWidget);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(5);

    recordButton_ =
        makeSecondaryButton(tr("Record"), QStringLiteral("recordButton"), true, rowWidget);
    connect(recordButton_, &QPushButton::clicked, this,
            [this]() { emit recordToggled(info_.callId); });

    volumeBox_ = new QWidget(rowWidget);
    auto *volLayout = new QHBoxLayout(volumeBox_);
    volLayout->setContentsMargins(0, 0, 0, 0);
    volLayout->setSpacing(3);
    auto *spkIcon = new QLabel(volumeBox_);
    spkIcon->setPixmap(Icons::speakers().pixmap(14, 14));
    volumeSlider_ = new QSlider(Qt::Horizontal, volumeBox_);
    volumeSlider_->setObjectName(QStringLiteral("volumeSlider"));
    volumeSlider_->setRange(0, 200);
    volumeSlider_->setValue(100);
    volumeSlider_->setToolTip(tr("Call speaker volume"));
    connect(volumeSlider_, &QSlider::valueChanged, this, [this](int val) {
        emit volumeChanged(info_.callId, static_cast<float>(val) / 100.0f);
    });
    volLayout->addWidget(spkIcon);
    volLayout->addWidget(volumeSlider_, 1);

    answerButton_ = makePrimaryButton(Icons::callStart(), tr("Answer"),
                                      QStringLiteral("answerButton"), QStringLiteral("#16a34a"),
                                      rowWidget);
    hangupButton_ = makePrimaryButton(Icons::callStop(), tr("Hang up"),
                                      QStringLiteral("hangupButton"), QStringLiteral("#dc2626"),
                                      rowWidget);
    connect(answerButton_, &QPushButton::clicked, this,
            [this]() { emit answerClicked(info_.callId); });
    connect(hangupButton_, &QPushButton::clicked, this,
            [this]() { emit hangupClicked(info_.callId); });

    row->addWidget(recordButton_);
    row->addWidget(volumeBox_, 1);
    row->addStretch();
    row->addWidget(answerButton_);
    row->addWidget(hangupButton_);
    return rowWidget;
}

int CallCard::callId() const { return info_.callId; }
const CallInfo &CallCard::callInfo() const { return info_; }

void CallCard::updateCall(const CallInfo &info) {
    info_ = info;
    refreshDisplay();
}

void CallCard::setActiveHighlight(bool active) {
    if (activeHighlight_ == active) {
        return;
    }
    activeHighlight_ = active;
    updateCardStyle();
}

void CallCard::setKeypadExpanded(bool expanded) {
    keypadButton_->setChecked(expanded);
    dtmfPad_->setVisible(expanded);
}

bool CallCard::isKeypadOpen() const {
    return info_.state == CallState::Confirmed && keypadButton_->isChecked();
}

void CallCard::appendDtmfEcho(const QString &digits) {
    if (digits.isEmpty()) {
        return;
    }
    echoedDigits_.append(digits);
    dtmfEchoLabel_->setText(tr("DTMF: %1").arg(echoedDigits_));
    dtmfEchoLabel_->setVisible(true);
}

QString CallCard::dtmfEcho() const { return echoedDigits_; }
QPushButton *CallCard::answerButton() const { return answerButton_; }
QPushButton *CallCard::hangupButton() const { return hangupButton_; }
QPushButton *CallCard::holdButton() const { return holdButton_; }
QPushButton *CallCard::muteButton() const { return muteButton_; }
QPushButton *CallCard::keypadButton() const { return keypadButton_; }
QPushButton *CallCard::transferButton() const { return transferButton_; }
QPushButton *CallCard::recordButton() const { return recordButton_; }
QSlider *CallCard::volumeSlider() const { return volumeSlider_; }
DtmfPad *CallCard::dtmfPad() const { return dtmfPad_; }

void CallCard::mousePressEvent(QMouseEvent *event) {
    emit activated(info_.callId);
    QFrame::mousePressEvent(event);
}

void CallCard::refreshDisplay() {
    remoteLabel_->setText(formatRemoteParty(info_));
    refreshControlStates();

    if (info_.state == CallState::Confirmed) {
        if (!localConfirmStart_.isValid()) {
            localConfirmStart_ =
                info_.connectedAt.isValid() ? info_.connectedAt : QDateTime::currentDateTime();
        }
        if (!durationTimer_->isActive()) {
            durationTimer_->start();
        }
    } else {
        durationTimer_->stop();
    }
    refreshStateText();
}

void CallCard::refreshControlStates() {
    const bool connected = (info_.state == CallState::Confirmed);
    inCallBox_->setVisible(connected);
    recordButton_->setVisible(connected);
    volumeBox_->setVisible(connected);
    answerButton_->setVisible(info_.isIncomingRinging());

    const QSignalBlocker blockHold(holdButton_);
    holdButton_->setChecked(info_.localHold);
    holdButton_->setText(info_.localHold ? tr("Resume") : tr("Hold"));

    const QSignalBlocker blockMute(muteButton_);
    muteButton_->setChecked(info_.muted);
    muteButton_->setText(info_.muted ? tr("Muted") : tr("Mute"));

    const QSignalBlocker blockRec(recordButton_);
    recordButton_->setChecked(info_.recording);
    recordButton_->setText(info_.recording ? tr("Recording") : tr("Record"));
    recordButton_->setStyleSheet(recordButtonStyle(info_.recording));

    if (!volumeSlider_->isSliderDown()) {
        const QSignalBlocker blockVol(volumeSlider_);
        volumeSlider_->setValue(static_cast<int>(std::lround(info_.rxLevel * 100.0f)));
    }
}

void CallCard::refreshStateText() {
    stateLabel_->setText(composeStatusLine());
}

QString CallCard::baseStateText() const {
    if (info_.isIncomingRinging()) {
        return tr("Incoming call");
    }
    switch (info_.state) {
    case CallState::Calling:
        return tr("Calling...");
    case CallState::Incoming:
        return tr("Incoming call");
    case CallState::Early:
        return tr("Ringing...");
    case CallState::Connecting:
        return tr("Connecting...");
    case CallState::Confirmed: {
        const QDateTime base = info_.connectedAt.isValid() ? info_.connectedAt : localConfirmStart_;
        const qint64 secs = base.secsTo(QDateTime::currentDateTime());
        return tr("Connected  %1").arg(formatDurationMmSs(secs));
    }
    case CallState::Disconnected:
        return tr("Ended");
    }
    return {};
}

QString CallCard::composeStatusLine() const {
    QStringList parts{baseStateText()};
    if (info_.inConference) {
        parts.append(tr("Conference"));
    }
    if (info_.localHold) {
        parts.append(tr("On hold"));
    }
    if (info_.remoteHold) {
        parts.append(tr("Remote hold"));
    }
    if (info_.muted) {
        parts.append(tr("Muted"));
    }
    if (info_.recording) {
        parts.append(tr("Recording"));
    }
    if (info_.transferStatus > 0) {
        if (info_.transferStatus < 200) {
            parts.append(tr("Transferring (%1)").arg(info_.transferStatus));
        } else if (info_.transferStatus < 300) {
            parts.append(tr("Transferred (%1)").arg(info_.transferStatus));
        } else {
            parts.append(tr("Transfer failed (%1)").arg(info_.transferStatus));
        }
    }
    return parts.join(QStringLiteral(" · "));
}

void CallCard::updateCardStyle() {
    const QString border = activeHighlight_ ? QStringLiteral("1.5px solid #2563eb")
                                            : QStringLiteral("1px solid #cbd5e1");
    setStyleSheet(
        QStringLiteral("QFrame#callCard { background-color: #f8fafc; border: %1; "
                       "border-radius: 6px; }")
            .arg(border));
}

} // namespace tincan::ui
