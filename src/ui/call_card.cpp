// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "ui/call_card.h"

#include "ui/icons.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

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
    const qint64 mins = clamped / 60;
    const qint64 secs = clamped % 60;
    return QStringLiteral("%1:%2")
        .arg(mins, 2, 10, QLatin1Char('0'))
        .arg(secs, 2, 10, QLatin1Char('0'));
}

QPushButton *makeCallActionButton(const QIcon &icon, const QString &text, const QString &objName,
                                  const QString &bgHex, QWidget *parent) {
    auto *btn = new QPushButton(icon, text, parent);
    btn->setObjectName(objName);
    btn->setStyleSheet(QStringLiteral("QPushButton { background-color: %1; color: #ffffff; "
                                      "font-weight: bold; border-radius: 4px; padding: 4px 10px; }")
                           .arg(bgHex));
    return btn;
}

} // namespace

CallCard::CallCard(const CallInfo &info, QWidget *parent) : QFrame(parent), info_(info) {
    setObjectName(QStringLiteral("callCard"));
    setFrameShape(QFrame::StyledPanel);
    setStyleSheet(QStringLiteral(
        "QFrame#callCard { background-color: #f8fafc; border: 1px solid #cbd5e1; "
        "border-radius: 6px; }"));

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(6);

    remoteLabel_ = new QLabel(this);
    remoteLabel_->setObjectName(QStringLiteral("remoteLabel"));
    QFont remoteFont = remoteLabel_->font();
    remoteFont.setBold(true);
    remoteLabel_->setFont(remoteFont);
    remoteLabel_->setWordWrap(true);

    stateLabel_ = new QLabel(this);
    stateLabel_->setObjectName(QStringLiteral("stateLabel"));
    stateLabel_->setStyleSheet(QStringLiteral("color: #475569;"));

    answerButton_ = makeCallActionButton(Icons::callStart(), tr("Answer"),
                                         QStringLiteral("answerButton"), QStringLiteral("#16a34a"),
                                         this);
    hangupButton_ = makeCallActionButton(Icons::callStop(), tr("Hang up"),
                                         QStringLiteral("hangupButton"), QStringLiteral("#dc2626"),
                                         this);
    connect(answerButton_, &QPushButton::clicked, this,
            [this]() { emit answerClicked(info_.callId); });
    connect(hangupButton_, &QPushButton::clicked, this,
            [this]() { emit hangupClicked(info_.callId); });

    auto *btnRow = new QHBoxLayout();
    btnRow->setSpacing(6);
    btnRow->addStretch();
    btnRow->addWidget(answerButton_);
    btnRow->addWidget(hangupButton_);

    root->addWidget(remoteLabel_);
    root->addWidget(stateLabel_);
    root->addLayout(btnRow);

    durationTimer_ = new QTimer(this);
    durationTimer_->setInterval(1000);
    connect(durationTimer_, &QTimer::timeout, this, &CallCard::refreshTimerText);
    refreshDisplay();
}

int CallCard::callId() const {
    return info_.callId;
}

void CallCard::updateCall(const CallInfo &info) {
    info_ = info;
    refreshDisplay();
}

QPushButton *CallCard::answerButton() const {
    return answerButton_;
}

QPushButton *CallCard::hangupButton() const {
    return hangupButton_;
}

void CallCard::refreshDisplay() {
    remoteLabel_->setText(formatRemoteParty(info_));
    answerButton_->setVisible(info_.isIncomingRinging());

    if (info_.state == CallState::Confirmed) {
        if (!localConfirmStart_.isValid()) {
            localConfirmStart_ =
                info_.connectedAt.isValid() ? info_.connectedAt : QDateTime::currentDateTime();
        }
        if (!durationTimer_->isActive()) {
            durationTimer_->start();
        }
        refreshTimerText();
    } else {
        durationTimer_->stop();
        stateLabel_->setText(stateLabelText());
    }
}

void CallCard::refreshTimerText() {
    const QDateTime base = info_.connectedAt.isValid() ? info_.connectedAt : localConfirmStart_;
    const qint64 secs = base.secsTo(QDateTime::currentDateTime());
    stateLabel_->setText(tr("Connected  %1").arg(formatDurationMmSs(secs)));
}

QString CallCard::stateLabelText() const {
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
    case CallState::Confirmed:
        return tr("Connected");
    case CallState::Disconnected:
        return tr("Ended");
    }
    return {};
}

} // namespace tincan::ui
