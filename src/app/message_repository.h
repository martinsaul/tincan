// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include "app/database.h"
#include "core/chat_message.h"

#include <QList>
#include <QString>

#include <optional>

namespace tincan {

class MessageRepository {
public:
    explicit MessageRepository(Database &database);

    qint64 insert(const ChatMessage &message);
    bool updateStatus(qint64 id, MessageStatus status);
    std::optional<ChatMessage> findById(qint64 id) const;
    QList<ChatMessage> listForPeer(const QString &peerUri, int limit = 500) const;
    QList<ConversationSummary> listConversations() const;
    bool markPeerRead(const QString &peerUri);
    int unreadCountForPeer(const QString &peerUri) const;
    int totalUnreadCount() const;
    bool deleteConversation(const QString &peerUri);

private:
    Database &database_;
};

} // namespace tincan
