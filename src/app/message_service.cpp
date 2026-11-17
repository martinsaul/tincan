// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "app/message_service.h"

#include "core/sip_uri.h"

#include <QDateTime>

namespace tincan {

namespace {

void setOutError(QString *error, const QString &message) {
    if (error != nullptr) {
        *error = message;
    }
}

} // namespace

MessageService::MessageService(ISipEngine &engine, AccountService &accountService,
                               MessageRepository &repository, QObject *parent)
    : QObject(parent), engine_(engine), accountService_(accountService), repository_(repository),
      cachedTotalUnread_(repository.totalUnreadCount()) {}

qint64 MessageService::sendMessage(const QString &peerOrNumber, const QString &body,
                                   const QString &accountId, QString *error) {
    if (body.trimmed().isEmpty()) {
        setOutError(error, tr("Message cannot be empty."));
        return -1;
    }

    const auto acc = accountId.trimmed().isEmpty() ? accountService_.defaultAccount()
                                                   : accountService_.account(accountId);
    if (!acc.has_value()) {
        setOutError(error, tr("No SIP account configured."));
        return -1;
    }

    const QString peerUri = resolvePeerUri(peerOrNumber, acc->id);
    if (peerUri.isEmpty()) {
        setOutError(error, tr("Invalid recipient address."));
        return -1;
    }

    ChatMessage msg;
    msg.accountId = acc->id;
    msg.peerUri = peerUri;
    msg.direction = MessageDirection::Outgoing;
    msg.body = body;
    msg.sentAt = QDateTime::currentDateTimeUtc();
    msg.status = MessageStatus::Pending;
    msg.read = true;

    const qint64 msgId = repository_.insert(msg);
    if (msgId <= 0) {
        setOutError(error, tr("Failed to store outgoing message."));
        return -1;
    }

    const quint64 token = nextToken_++;
    pendingTokens_.insert(token, PendingSend{msgId, peerUri});

    if (!engine_.sendMessage(acc->id, peerUri, body, token)) {
        pendingTokens_.remove(token);
        (void)repository_.updateStatus(msgId, MessageStatus::Failed);
        emit messageStatusUpdated(msgId, peerUri, MessageStatus::Failed, 503,
                                  tr("Failed to send message."));
    }

    emit messagesChanged(peerUri);
    emit conversationsChanged();
    return msgId;
}

void MessageService::markPeerRead(const QString &peerOrNumber) {
    const QString peerUri = resolvePeerUri(peerOrNumber);
    if (peerUri.isEmpty() || repository_.unreadCountForPeer(peerUri) == 0 ||
        !repository_.markPeerRead(peerUri)) {
        return;
    }
    notifyUnreadIfChanged();
    emit messagesChanged(peerUri);
    emit conversationsChanged();
}

bool MessageService::deleteConversation(const QString &peerOrNumber) {
    const QString peerUri = resolvePeerUri(peerOrNumber);
    if (peerUri.isEmpty() || !repository_.deleteConversation(peerUri)) {
        return false;
    }
    notifyUnreadIfChanged();
    emit messagesChanged(peerUri);
    emit conversationsChanged();
    return true;
}

QList<ChatMessage> MessageService::messagesForPeer(const QString &peerOrNumber, int limit) const {
    const QString peerUri = resolvePeerUri(peerOrNumber);
    return peerUri.isEmpty() ? QList<ChatMessage>{} : repository_.listForPeer(peerUri, limit);
}

QList<ConversationSummary> MessageService::conversations() const {
    return repository_.listConversations();
}

int MessageService::unreadCountForPeer(const QString &peerOrNumber) const {
    const QString peerUri = resolvePeerUri(peerOrNumber);
    return peerUri.isEmpty() ? 0 : repository_.unreadCountForPeer(peerUri);
}

int MessageService::totalUnreadCount() const {
    return repository_.totalUnreadCount();
}

QString MessageService::resolvePeerUri(const QString &peerOrNumber,
                                       const QString &accountId) const {
    const QString trimmed = peerOrNumber.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }
    const auto acc = accountId.trimmed().isEmpty() ? accountService_.defaultAccount()
                                                   : accountService_.account(accountId);
    const QString full =
        acc.has_value() ? SipUri::toSipUri(trimmed, *acc) : SipUri::toSipUri(trimmed, AccountConfig{});
    const QString bare = SipUri::bareSipUri(full.isEmpty() ? trimmed : full);
    return bare.isEmpty() ? full : bare;
}

void MessageService::onIncomingMessage(const MessageInfo &info) {
    const QString peerUri = resolvePeerUri(info.fromUri, info.accountId);
    if (peerUri.isEmpty()) {
        return;
    }

    ChatMessage msg;
    msg.accountId = info.accountId;
    msg.peerUri = peerUri;
    msg.direction = MessageDirection::Incoming;
    msg.body = info.body;
    msg.sentAt =
        info.receivedAt.isValid() ? info.receivedAt.toUTC() : QDateTime::currentDateTimeUtc();
    msg.status = MessageStatus::Received;
    msg.read = false;

    const qint64 id = repository_.insert(msg);
    if (id <= 0) {
        return;
    }
    msg.id = id;

    notifyUnreadIfChanged();
    emit messagesChanged(peerUri);
    emit conversationsChanged();
    emit incomingMessageReceived(msg, info.fromName);
}

void MessageService::onMessageStatus(quint64 token, int statusCode, const QString &reason) {
    if (statusCode >= 100 && statusCode < 200) {
        return;
    }
    const auto it = pendingTokens_.find(token);
    if (it == pendingTokens_.end()) {
        return;
    }
    const PendingSend pending = *it;
    pendingTokens_.erase(it);

    const MessageStatus nextStatus =
        (statusCode >= 200 && statusCode < 300) ? MessageStatus::Sent : MessageStatus::Failed;
    (void)repository_.updateStatus(pending.messageId, nextStatus);

    emit messageStatusUpdated(pending.messageId, pending.peerUri, nextStatus, statusCode, reason);
    emit messagesChanged(pending.peerUri);
    emit conversationsChanged();
}

void MessageService::notifyUnreadIfChanged() {
    const int current = repository_.totalUnreadCount();
    if (current != cachedTotalUnread_) {
        cachedTotalUnread_ = current;
        emit unreadCountChanged(current);
    }
}

} // namespace tincan
