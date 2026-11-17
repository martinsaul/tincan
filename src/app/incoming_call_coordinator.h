// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include "app/settings_store.h"
#include "core/call_info.h"

#include <QMap>
#include <QObject>
#include <QString>

class QTimer;

namespace tincan {

class IncomingCallCoordinator : public QObject {
    Q_OBJECT

public:
    enum class ImmediateOutcome { Ring, Rejected486, Redirected };

    struct Evaluation {
        ImmediateOutcome outcome = ImmediateOutcome::Ring;
        QString redirectTarget;
    };

    explicit IncomingCallCoordinator(SettingsStore &settings, QObject *parent = nullptr);

    Evaluation handleIncoming(const CallInfo &call, bool isBusy);
    void cancelTimers(int callId);

signals:
    void noAnswerTimeout(int callId, const QString &accountId, const QString &target);
    void autoAnswerDue(int callId);

private:
    void scheduleNoAnswer(int callId, const QString &accountId, const QString &target,
                          int delaySec);
    void scheduleAutoAnswer(int callId, int delaySec);

    SettingsStore &settings_;
    QMap<int, QTimer *> noAnswerTimers_;
    QMap<int, QTimer *> autoAnswerTimers_;
};

} // namespace tincan
