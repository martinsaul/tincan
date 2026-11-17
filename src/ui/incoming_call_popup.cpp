// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "ui/incoming_call_popup.h"

#include "ui/icons.h"

#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScreen>
#include <QVBoxLayout>

namespace tincan::ui {

namespace {

QString formatPopupCaller(const CallInfo &info) {
    const QString name = info.remoteDisplayName.trimmed();
    const QString uri = info.remoteUri.trimmed();
    if (!name.isEmpty() && !uri.isEmpty()) {
        return QStringLiteral("%1\n%2").arg(name, uri);
    }
    return name.isEmpty() ? uri : name;
}

QPushButton *makePopupButton(const QIcon &icon, const QString &text, const QString &objName,
                             const QString &bgHex, QWidget *parent) {
    auto *btn = new QPushButton(icon, text, parent);
    btn->setObjectName(objName);
    btn->setStyleSheet(QStringLiteral("QPushButton { background-color: %1; color: #ffffff; "
                                      "font-weight: bold; border-radius: 5px; padding: 6px 12px; }")
                           .arg(bgHex));
    return btn;
}

} // namespace

IncomingCallPopup::IncomingCallPopup(CallService &callService, QWidget *parent)
    : QFrame(parent, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint),
      callService_(callService) {
    setObjectName(QStringLiteral("incomingCallPopup"));
    setFixedWidth(280);
    setStyleSheet(QStringLiteral(
        "QFrame#incomingCallPopup { background-color: #1e293b; color: #f8fafc; "
        "border: 1px solid #475569; border-radius: 8px; }"));

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(12, 10, 12, 10);
    root->setSpacing(8);

    titleLabel_ = new QLabel(tr("Incoming Call"), this);
    titleLabel_->setStyleSheet(
        QStringLiteral("color: #94a3b8; font-size: 11px; font-weight: bold;"));

    callerLabel_ = new QLabel(this);
    callerLabel_->setObjectName(QStringLiteral("popupCallerLabel"));
    callerLabel_->setStyleSheet(
        QStringLiteral("color: #f8fafc; font-size: 13px; font-weight: bold;"));
    callerLabel_->setWordWrap(true);

    answerButton_ = makePopupButton(Icons::callStart(), tr("Answer"),
                                    QStringLiteral("popupAnswerButton"), QStringLiteral("#16a34a"),
                                    this);
    rejectButton_ = makePopupButton(Icons::callStop(), tr("Reject"),
                                    QStringLiteral("popupRejectButton"), QStringLiteral("#dc2626"),
                                    this);
    connect(answerButton_, &QPushButton::clicked, this, [this]() {
        if (currentCallId_ >= 0) {
            (void)callService_.answer(currentCallId_);
        }
    });
    connect(rejectButton_, &QPushButton::clicked, this, [this]() {
        if (currentCallId_ >= 0) {
            (void)callService_.reject(currentCallId_);
        }
    });

    auto *btnRow = new QHBoxLayout();
    btnRow->setSpacing(8);
    btnRow->addWidget(answerButton_, 1);
    btnRow->addWidget(rejectButton_, 1);

    root->addWidget(titleLabel_);
    root->addWidget(callerLabel_);
    root->addLayout(btnRow);

    connect(&callService_, &CallService::incomingCall, this, &IncomingCallPopup::onIncomingCall);
    connect(&callService_, &CallService::callUpdated, this,
            [this](const CallInfo &) { onCallChangedOrRemoved(); });
    connect(&callService_, &CallService::callRemoved, this,
            [this](const CallInfo &) { onCallChangedOrRemoved(); });
}

int IncomingCallPopup::currentCallId() const {
    return currentCallId_;
}

void IncomingCallPopup::onIncomingCall(const CallInfo &info) {
    showForCall(info);
}

void IncomingCallPopup::onCallChangedOrRemoved() {
    for (const CallInfo &c : callService_.calls()) {
        if (c.callId == currentCallId_ && c.isIncomingRinging()) {
            showForCall(c);
            return;
        }
    }
    for (const CallInfo &c : callService_.calls()) {
        if (c.isIncomingRinging()) {
            showForCall(c);
            return;
        }
    }
    currentCallId_ = -1;
    hide();
}

void IncomingCallPopup::showForCall(const CallInfo &info) {
    currentCallId_ = info.callId;
    callerLabel_->setText(formatPopupCaller(info));
    adjustSize();
    positionBottomRight();
    show();
    raise();
}

void IncomingCallPopup::positionBottomRight() {
    QScreen *screen = QGuiApplication::primaryScreen();
    if (screen == nullptr) {
        return;
    }
    const QRect avail = screen->availableGeometry();
    const int margin = 16;
    move(avail.right() - width() - margin, avail.bottom() - height() - margin);
}

} // namespace tincan::ui
