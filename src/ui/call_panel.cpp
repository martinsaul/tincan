// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "ui/call_panel.h"

#include "ui/call_card.h"

#include <QVBoxLayout>

namespace tincan::ui {

CallPanel::CallPanel(CallService &callService, QWidget *parent)
    : QWidget(parent), callService_(callService) {
    setObjectName(QStringLiteral("callPanel"));
    layout_ = new QVBoxLayout(this);
    layout_->setContentsMargins(8, 4, 8, 4);
    layout_->setSpacing(6);

    connect(&callService_, &CallService::callAdded, this, &CallPanel::onCallAdded);
    connect(&callService_, &CallService::callUpdated, this, &CallPanel::onCallUpdated);
    connect(&callService_, &CallService::callRemoved, this, &CallPanel::onCallRemoved);

    for (const CallInfo &call : callService_.calls()) {
        onCallAdded(call);
    }
    syncVisibility();
}

QList<CallCard *> CallPanel::cards() const {
    return cards_.values();
}

CallCard *CallPanel::cardForCall(int callId) const {
    return cards_.value(callId, nullptr);
}

void CallPanel::onCallAdded(const CallInfo &info) {
    if (cards_.contains(info.callId)) {
        cards_[info.callId]->updateCall(info);
        return;
    }

    auto *card = new CallCard(info, this);
    connect(card, &CallCard::answerClicked, this,
            [this](int id) { (void)callService_.answer(id); });
    connect(card, &CallCard::hangupClicked, this, &CallPanel::handleHangupOrReject);

    cards_.insert(info.callId, card);
    layout_->addWidget(card);
    syncVisibility();
}

void CallPanel::onCallUpdated(const CallInfo &info) {
    if (CallCard *card = cards_.value(info.callId, nullptr)) {
        card->updateCall(info);
    } else {
        onCallAdded(info);
    }
}

void CallPanel::onCallRemoved(const CallInfo &info) {
    if (CallCard *card = cards_.take(info.callId)) {
        layout_->removeWidget(card);
        card->deleteLater();
    }
    syncVisibility();
}

void CallPanel::handleHangupOrReject(int callId) {
    for (const CallInfo &c : callService_.calls()) {
        if (c.callId == callId && c.isIncomingRinging()) {
            (void)callService_.reject(callId);
            return;
        }
    }
    (void)callService_.hangup(callId);
}

void CallPanel::syncVisibility() {
    setVisible(!cards_.isEmpty());
}

} // namespace tincan::ui
