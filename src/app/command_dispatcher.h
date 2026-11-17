// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include "app/call_service.h"
#include "app/command_line.h"

#include <QObject>

namespace tincan {

// Carries out a CommandLine in the primary instance. Call actions go through
// CallService; window actions are re-emitted as signals for the UI layer.
class CommandDispatcher : public QObject {
    Q_OBJECT

public:
    explicit CommandDispatcher(CallService &callService, QObject *parent = nullptr);

    // Returns false when the command could not be carried out.
    bool dispatch(const tincan::CommandLine &command);

signals:
    void showRequested();
    // A link or command-line target to put in the dial field. It is never
    // dialled automatically: any web page can open a sip:/tel: link.
    void dialPrefillRequested(const QString &target);
    void quitRequested();

private:
    bool dial(const QString &target);
    bool answerFirstRinging();
    bool hangupActive();

    CallService &callService_;
};

} // namespace tincan
