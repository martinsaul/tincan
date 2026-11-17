// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include "app/account_service.h"
#include "app/message_repository.h"
#include "core/chat_message.h"
#include "core/message_info.h"
#include "core/sip_engine.h"
#include "core/sip_events.h"

#include <QHash>
#include <QList>
#include <QObject>
#include <QString>

namespace tincan {

class MessageService : public QObject, public ISipEvents {
    Q_OBJECT

public:
    MessageService(ISipEngine &engine, AccountService &accountService,
                   MessageRepository &repository, QObject *parent = nullptr);

    qint64 sendMessage(const QString &peerOrNumber, const QString &body,
                       const QString &accountId = {}, QString *error = nullptr);
    void markPeerRead(const QString &peerOrNumber);
    bool deleteConversation(const QString &peerOrNumber);

    QList<ChatMessage> messagesForPeer(const QString &peerOrNumber, int limit = 500) const;
    QList<ConversationSummary> conversations() const;
    int unreadCountForPeer(const QString &peerOrNumber) const;
    int totalUnreadCount() const;
    QString resolvePeerUri(const QString &peerOrNumber, const QString &accountId = {}) const;

    void onIncomingMessage(const MessageInfo &info) override;
    void onMessageStatus(quint64 token, int statusCode, const QString &reason) override;

signals:
    void messagesChanged(const QString &peerUri);
    void conversationsChanged();
    void unreadCountChanged(int totalUnread);
    void incomingMessageReceived(const tincan::ChatMessage &message, const QString &fromName);
    void messageStatusUpdated(qint64 messageId, const QString &peerUri,
                              tincan::MessageStatus status, int statusCode, const QString &reason);

private:
    struct PendingSend {
        qint64 messageId = 0;
        QString peerUri;
    };

    void notifyUnreadIfChanged();

    ISipEngine &engine_;
    AccountService &accountService_;
    MessageRepository &repository_;
    QHash<quint64, PendingSend> pendingTokens_;
    quint64 nextToken_ = 1;
    int cachedTotalUnread_ = 0;
};

} // namespace tincan
