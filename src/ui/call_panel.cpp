// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "ui/call_panel.h"

#include "ui/call_card.h"
#include "ui/transfer_dialog.h"

#include <QHBoxLayout>
#include <QPushButton>
#include <QVBoxLayout>

namespace tincan::ui {

CallPanel::CallPanel(CallService &callService, ContactService *contactService, QWidget *parent)
    : QWidget(parent), callService_(callService), contactService_(contactService) {
    setObjectName(QStringLiteral("callPanel"));
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(8, 4, 8, 4);
    root->setSpacing(6);

    auto *headerRow = new QHBoxLayout();
    headerRow->addStretch();
    mergeButton_ = new QPushButton(tr("Merge"), this);
    mergeButton_->setObjectName(QStringLiteral("mergeCallsButton"));
    mergeButton_->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #0284c7; color: #ffffff; font-weight: bold; "
        "border-radius: 4px; padding: 3px 10px; font-size: 11px; }"));
    mergeButton_->setVisible(false);
    connect(mergeButton_, &QPushButton::clicked, this,
            [this]() { (void)callService_.mergeAllCalls(); });
    headerRow->addWidget(mergeButton_);
    root->addLayout(headerRow);

    cardsLayout_ = new QVBoxLayout();
    cardsLayout_->setSpacing(6);
    root->addLayout(cardsLayout_);

    connect(&callService_, &CallService::callAdded, this, &CallPanel::onCallAdded);
    connect(&callService_, &CallService::callUpdated, this, &CallPanel::onCallUpdated);
    connect(&callService_, &CallService::callRemoved, this, &CallPanel::onCallRemoved);
    connect(&callService_, &CallService::activeCallChanged, this,
            [this](int) { refreshActiveHighlights(); });
    connect(&callService_, &CallService::dtmfSent, this, [this](int id, const QString &digits) {
        if (CallCard *card = cards_.value(id, nullptr)) {
            card->appendDtmfEcho(digits);
        }
    });
    if (contactService_ != nullptr) {
        connect(contactService_, &ContactService::contactsChanged, this,
                &CallPanel::refreshContactNames);
    }

    for (const CallInfo &call : callService_.calls()) {
        onCallAdded(call);
    }
    syncVisibility();
}

QList<CallCard *> CallPanel::cards() const { return cards_.values(); }
CallCard *CallPanel::cardForCall(int callId) const { return cards_.value(callId, nullptr); }
QPushButton *CallPanel::mergeButton() const { return mergeButton_; }
bool CallPanel::isInCallKeypadShown() const { return keypadShown_; }

CallInfo CallPanel::enrichWithContactName(CallInfo info) const {
    if (contactService_ != nullptr) {
        const QString resolved = contactService_->resolveDisplayName(info.remoteUri);
        if (!resolved.isEmpty()) {
            info.remoteDisplayName = resolved;
        }
    }
    return info;
}

void CallPanel::refreshContactNames() {
    for (const CallInfo &call : callService_.calls()) {
        if (CallCard *card = cards_.value(call.callId, nullptr)) {
            card->updateCall(enrichWithContactName(call));
        }
    }
}

void CallPanel::onCallAdded(const CallInfo &info) {
    const CallInfo enriched = enrichWithContactName(info);
    if (cards_.contains(enriched.callId)) {
        cards_[enriched.callId]->updateCall(enriched);
        syncVisibility();
        return;
    }

    auto *card = new CallCard(enriched, this);
    connectCardSignals(card);
    cards_.insert(enriched.callId, card);
    cardsLayout_->addWidget(card);
    refreshActiveHighlights();
    syncVisibility();
}

void CallPanel::onCallUpdated(const CallInfo &info) {
    if (CallCard *card = cards_.value(info.callId, nullptr)) {
        card->updateCall(enrichWithContactName(info));
        refreshActiveHighlights();
        syncVisibility();
    } else {
        onCallAdded(info);
    }
}

void CallPanel::onCallRemoved(const CallInfo &info) {
    if (CallCard *card = cards_.take(info.callId)) {
        cardsLayout_->removeWidget(card);
        card->deleteLater();
    }
    refreshActiveHighlights();
    syncVisibility();
}

void CallPanel::connectCardSignals(CallCard *card) {
    connect(card, &CallCard::answerClicked, this,
            [this](int id) { (void)callService_.answer(id); });
    connect(card, &CallCard::hangupClicked, this, &CallPanel::handleHangupOrReject);
    connect(card, &CallCard::holdToggled, this,
            [this](int id, bool hold) { (void)callService_.setHold(id, hold); });
    connect(card, &CallCard::muteToggled, this,
            [this](int id, bool mute) { (void)callService_.setMute(id, mute); });
    connect(card, &CallCard::dtmfDigitPressed, this,
            [this](int id, const QString &digit) { (void)callService_.sendDtmf(id, digit); });
    connect(card, &CallCard::transferClicked, this, &CallPanel::openTransferDialog);
    connect(card, &CallCard::keypadToggled, this, [this](int, bool) { refreshKeypadShown(); });
    connect(card, &CallCard::recordToggled, this,
            [this](int id) { (void)callService_.toggleRecording(id); });
    connect(card, &CallCard::volumeChanged, this, [this](int id, float rx) {
        (void)callService_.setCallVolume(id, rx, 1.0f);
    });
    connect(card, &CallCard::activated, this, [this](int id) {
        if (cards_.size() >= 2 && callService_.activeCallId() != id) {
            (void)callService_.activateCall(id);
        }
    });
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

void CallPanel::openTransferDialog(int callId) {
    QList<CallInfo> others;
    for (const CallInfo &c : callService_.calls()) {
        if (c.callId != callId && c.state == CallState::Confirmed) {
            others.append(enrichWithContactName(c));
        }
    }

    TransferDialog dlg(others, window());
    if (dlg.exec() != QDialog::Accepted) {
        return;
    }
    if (dlg.isAttended()) {
        (void)callService_.transferAttended(callId, dlg.attendedTargetCallId());
    } else {
        (void)callService_.transferBlind(callId, dlg.blindTarget());
    }
}

void CallPanel::refreshActiveHighlights() {
    const int activeId = callService_.activeCallId();
    const bool showBorder = cards_.size() >= 2;
    for (auto it = cards_.begin(); it != cards_.end(); ++it) {
        it.value()->setActiveHighlight(showBorder && it.key() == activeId);
    }
}

void CallPanel::syncVisibility() {
    mergeButton_->setVisible(cards_.size() >= 2);
    setVisible(!cards_.isEmpty());
    refreshKeypadShown();
}

void CallPanel::refreshKeypadShown() {
    bool shown = false;
    for (const CallCard *card : std::as_const(cards_)) {
        shown = shown || card->isKeypadOpen();
    }
    if (shown != keypadShown_) {
        keypadShown_ = shown;
        emit inCallKeypadShownChanged(shown);
    }
}

} // namespace tincan::ui
