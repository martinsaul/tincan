// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include <QDateTime>
#include <QMetaType>
#include <QString>

namespace tincan {

enum class MessageDirection {
    Outgoing = 0,
    Incoming = 1,
};

enum class MessageStatus {
    Pending = 0,
    Sent = 1,
    Failed = 2,
    Received = 3,
};

struct ChatMessage {
    qint64 id = 0;
    QString accountId;
    QString peerUri;
    MessageDirection direction = MessageDirection::Outgoing;
    QString body;
    QDateTime sentAt;
    MessageStatus status = MessageStatus::Pending;
    bool read = false;
};

struct ConversationSummary {
    QString accountId;
    QString peerUri;
    QString lastBody;
    QDateTime lastSentAt;
    MessageStatus lastStatus = MessageStatus::Received;
    int unreadCount = 0;
};

} // namespace tincan

Q_DECLARE_METATYPE(tincan::MessageDirection)
Q_DECLARE_METATYPE(tincan::MessageStatus)
Q_DECLARE_METATYPE(tincan::ChatMessage)
Q_DECLARE_METATYPE(tincan::ConversationSummary)
