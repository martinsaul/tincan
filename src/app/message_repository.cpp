// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "app/message_repository.h"

#include <QDateTime>
#include <QSqlQuery>
#include <QVariant>

namespace tincan {

namespace {

QString nonNull(const QString &s) {
    return s.isNull() ? QStringLiteral("") : s;
}

QString formatUtcIso(const QDateTime &dt) {
    const QDateTime effective = dt.isValid() ? dt.toUTC() : QDateTime::currentDateTimeUtc();
    return effective.toString(Qt::ISODateWithMs);
}

QDateTime parseUtcIso(const QString &text) {
    if (text.isEmpty()) {
        return {};
    }
    QDateTime dt = QDateTime::fromString(text, Qt::ISODateWithMs);
    if (!dt.isValid()) {
        dt = QDateTime::fromString(text, Qt::ISODate);
    }
    return dt;
}

MessageStatus intToMessageStatus(int value) {
    switch (value) {
    case 0:
        return MessageStatus::Pending;
    case 1:
        return MessageStatus::Sent;
    case 2:
        return MessageStatus::Failed;
    case 3:
        return MessageStatus::Received;
    default:
        return MessageStatus::Pending;
    }
}

ChatMessage readMessageRow(const QSqlQuery &query) {
    ChatMessage m;
    m.id = query.value(0).toLongLong();
    m.accountId = query.value(1).toString();
    m.peerUri = query.value(2).toString();
    m.direction =
        query.value(3).toInt() == 1 ? MessageDirection::Incoming : MessageDirection::Outgoing;
    m.body = query.value(4).toString();
    m.sentAt = parseUtcIso(query.value(5).toString());
    m.status = intToMessageStatus(query.value(6).toInt());
    m.read = query.value(7).toInt() != 0;
    return m;
}

} // namespace

MessageRepository::MessageRepository(Database &database) : database_(database) {}

qint64 MessageRepository::insert(const ChatMessage &message) {
    QSqlQuery query(database_.connection());
    if (!query.prepare(QStringLiteral(
            "INSERT INTO messages(account_id, peer_uri, direction, body, sent_at, status, read) "
            "VALUES (:account_id, :peer_uri, :direction, :body, :sent_at, :status, :read)"))) {
        return -1;
    }

    query.bindValue(QStringLiteral(":account_id"), nonNull(message.accountId));
    query.bindValue(QStringLiteral(":peer_uri"), nonNull(message.peerUri));
    query.bindValue(QStringLiteral(":direction"),
                    message.direction == MessageDirection::Incoming ? 1 : 0);
    query.bindValue(QStringLiteral(":body"), nonNull(message.body));
    query.bindValue(QStringLiteral(":sent_at"), formatUtcIso(message.sentAt));
    query.bindValue(QStringLiteral(":status"), static_cast<int>(message.status));
    query.bindValue(QStringLiteral(":read"), message.read ? 1 : 0);

    if (!query.exec()) {
        return -1;
    }
    return query.lastInsertId().toLongLong();
}

bool MessageRepository::updateStatus(qint64 id, MessageStatus status) {
    QSqlQuery query(database_.connection());
    if (!query.prepare(QStringLiteral("UPDATE messages SET status = :status WHERE id = :id"))) {
        return false;
    }
    query.bindValue(QStringLiteral(":status"), static_cast<int>(status));
    query.bindValue(QStringLiteral(":id"), id);
    return query.exec() && query.numRowsAffected() > 0;
}

std::optional<ChatMessage> MessageRepository::findById(qint64 id) const {
    QSqlQuery query(database_.connection());
    if (!query.prepare(QStringLiteral(
            "SELECT id, account_id, peer_uri, direction, body, sent_at, status, read "
            "FROM messages WHERE id = :id"))) {
        return std::nullopt;
    }
    query.bindValue(QStringLiteral(":id"), id);
    if (!query.exec() || !query.next()) {
        return std::nullopt;
    }
    return readMessageRow(query);
}

QList<ChatMessage> MessageRepository::listForPeer(const QString &peerUri, int limit) const {
    QSqlQuery query(database_.connection());
    if (!query.prepare(QStringLiteral(
            "SELECT id, account_id, peer_uri, direction, body, sent_at, status, read "
            "FROM ("
            "    SELECT id, account_id, peer_uri, direction, body, sent_at, status, read "
            "    FROM messages "
            "    WHERE peer_uri = :peer_uri "
            "    ORDER BY sent_at DESC, id DESC "
            "    LIMIT :limit"
            ") "
            "ORDER BY sent_at ASC, id ASC"))) {
        return {};
    }
    const int clampedLimit = limit > 0 ? limit : 500;
    query.bindValue(QStringLiteral(":peer_uri"), peerUri);
    query.bindValue(QStringLiteral(":limit"), clampedLimit);
    if (!query.exec()) {
        return {};
    }
    QList<ChatMessage> results;
    while (query.next()) {
        results.append(readMessageRow(query));
    }
    return results;
}

QList<ConversationSummary> MessageRepository::listConversations() const {
    QSqlQuery query(database_.connection());
    if (!query.prepare(QStringLiteral(
            "SELECT m.account_id, m.peer_uri, m.body, m.sent_at, m.status, "
            "       (SELECT COUNT(*) FROM messages u "
            "        WHERE u.peer_uri = m.peer_uri AND u.direction = 1 AND u.read = 0) "
            "FROM messages m "
            "WHERE m.id = ("
            "    SELECT m2.id FROM messages m2 "
            "    WHERE m2.peer_uri = m.peer_uri "
            "    ORDER BY m2.sent_at DESC, m2.id DESC "
            "    LIMIT 1"
            ") "
            "ORDER BY m.sent_at DESC, m.id DESC"))) {
        return {};
    }
    if (!query.exec()) {
        return {};
    }
    QList<ConversationSummary> list;
    while (query.next()) {
        ConversationSummary s;
        s.accountId = query.value(0).toString();
        s.peerUri = query.value(1).toString();
        s.lastBody = query.value(2).toString();
        s.lastSentAt = parseUtcIso(query.value(3).toString());
        s.lastStatus = intToMessageStatus(query.value(4).toInt());
        s.unreadCount = query.value(5).toInt();
        list.append(s);
    }
    return list;
}

bool MessageRepository::markPeerRead(const QString &peerUri) {
    QSqlQuery query(database_.connection());
    if (!query.prepare(QStringLiteral(
            "UPDATE messages SET read = 1 WHERE peer_uri = :peer_uri AND read = 0"))) {
        return false;
    }
    query.bindValue(QStringLiteral(":peer_uri"), peerUri);
    return query.exec();
}

int MessageRepository::unreadCountForPeer(const QString &peerUri) const {
    QSqlQuery query(database_.connection());
    if (!query.prepare(QStringLiteral(
            "SELECT COUNT(*) FROM messages "
            "WHERE peer_uri = :peer_uri AND direction = 1 AND read = 0"))) {
        return 0;
    }
    query.bindValue(QStringLiteral(":peer_uri"), peerUri);
    if (!query.exec() || !query.next()) {
        return 0;
    }
    return query.value(0).toInt();
}

int MessageRepository::totalUnreadCount() const {
    QSqlQuery query(database_.connection());
    if (!query.prepare(
            QStringLiteral("SELECT COUNT(*) FROM messages WHERE direction = 1 AND read = 0")) ||
        !query.exec() || !query.next()) {
        return 0;
    }
    return query.value(0).toInt();
}

bool MessageRepository::deleteConversation(const QString &peerUri) {
    QSqlQuery query(database_.connection());
    if (!query.prepare(QStringLiteral("DELETE FROM messages WHERE peer_uri = :peer_uri"))) {
        return false;
    }
    query.bindValue(QStringLiteral(":peer_uri"), peerUri);
    return query.exec();
}

} // namespace tincan
