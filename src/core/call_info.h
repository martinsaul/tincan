// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include <QDateTime>
#include <QMap>
#include <QString>

namespace tincan {

enum class CallDirection { Outgoing, Incoming };

enum class CallState { Calling, Incoming, Early, Connecting, Confirmed, Disconnected };

struct CallInfo {
    int callId = -1;
    QString accountId;
    CallDirection direction = CallDirection::Outgoing;
    CallState state = CallState::Calling;
    QString remoteUri;
    QString remoteDisplayName;
    int lastStatusCode = 0;
    QString lastReason;
    QDateTime connectedAt;

    bool localHold = false;
    bool remoteHold = false;
    bool muted = false;
    bool recording = false;
    bool inConference = false;
    int transferStatus = 0;
    float rxLevel = 1.0f;
    float txLevel = 1.0f;
    QMap<QString, QString> incomingHeaders;

    bool isIncomingRinging() const {
        return direction == CallDirection::Incoming &&
               (state == CallState::Incoming || state == CallState::Early);
    }
};

} // namespace tincan
