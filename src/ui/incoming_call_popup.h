// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include "app/call_service.h"
#include "app/contact_service.h"

#include <QFrame>
#include <QString>

class QLabel;
class QPushButton;

namespace tincan::ui {

class IncomingCallPopup : public QFrame {
    Q_OBJECT

public:
    explicit IncomingCallPopup(CallService &callService, ContactService *contactService = nullptr,
                               QWidget *parent = nullptr);

    int currentCallId() const;
    QString callerText() const;

private:
    void onIncomingCall(const CallInfo &info);
    void onCallChangedOrRemoved();
    void showForCall(const CallInfo &info);
    QString formatCaller(const CallInfo &info) const;
    void positionBottomRight();

    CallService &callService_;
    ContactService *contactService_ = nullptr;
    int currentCallId_ = -1;
    QLabel *titleLabel_ = nullptr;
    QLabel *callerLabel_ = nullptr;
    QPushButton *answerButton_ = nullptr;
    QPushButton *rejectButton_ = nullptr;
};

} // namespace tincan::ui
