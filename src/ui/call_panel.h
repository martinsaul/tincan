// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include "app/call_service.h"
#include "app/contact_service.h"

#include <QMap>
#include <QWidget>

class QPushButton;
class QVBoxLayout;

namespace tincan::ui {

class CallCard;

class CallPanel : public QWidget {
    Q_OBJECT

public:
    explicit CallPanel(CallService &callService, ContactService *contactService = nullptr,
                       QWidget *parent = nullptr);

    QList<CallCard *> cards() const;
    CallCard *cardForCall(int callId) const;
    QPushButton *mergeButton() const;
    // True while any connected call shows its in-call keypad.
    bool isInCallKeypadShown() const;

signals:
    void inCallKeypadShownChanged(bool shown);
    // An in-call keypad key was pressed (the DTMF has already been sent).
    void keypadDigitPressed(const QString &digit);

private:
    CallInfo enrichWithContactName(CallInfo info) const;
    void refreshContactNames();
    void onCallAdded(const CallInfo &info);
    void onCallUpdated(const CallInfo &info);
    void onCallRemoved(const CallInfo &info);
    void connectCardSignals(CallCard *card);
    void handleHangupOrReject(int callId);
    void openTransferDialog(int callId);
    void refreshActiveHighlights();
    void syncVisibility();
    void refreshKeypadShown();

    CallService &callService_;
    ContactService *contactService_ = nullptr;
    QPushButton *mergeButton_ = nullptr;
    QVBoxLayout *cardsLayout_ = nullptr;
    QMap<int, CallCard *> cards_;
    bool keypadShown_ = false;
};

} // namespace tincan::ui
