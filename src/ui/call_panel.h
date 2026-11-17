// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include "app/call_service.h"

#include <QMap>
#include <QWidget>

class QVBoxLayout;

namespace tincan::ui {

class CallCard;

class CallPanel : public QWidget {
    Q_OBJECT

public:
    explicit CallPanel(CallService &callService, QWidget *parent = nullptr);

    QList<CallCard *> cards() const;
    CallCard *cardForCall(int callId) const;

private:
    void onCallAdded(const CallInfo &info);
    void onCallUpdated(const CallInfo &info);
    void onCallRemoved(const CallInfo &info);
    void handleHangupOrReject(int callId);
    void syncVisibility();

    CallService &callService_;
    QVBoxLayout *layout_ = nullptr;
    QMap<int, CallCard *> cards_;
};

} // namespace tincan::ui
