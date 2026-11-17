// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "app/history_repository.h"

#include <QDateTime>
#include <QMetaType>
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

QString buildLikePattern(const QString &raw) {
    const QString trimmed = raw.trimmed();
    if (trimmed.isEmpty()) {
        return QStringLiteral("");
    }
    QString escaped;
    escaped.reserve(trimmed.size() + 4);
    escaped.append(u'%');
    for (const QChar ch : trimmed) {
        if (ch == u'\\' || ch == u'%' || ch == u'_') {
            escaped.append(u'\\');
        }
        escaped.append(ch);
    }
    escaped.append(u'%');
    return escaped;
}

HistoryEntry readHistoryRow(const QSqlQuery &query) {
    HistoryEntry e;
    e.id = query.value(0).toLongLong();
    e.accountId = query.value(1).toString();
    e.direction = query.value(2).toInt() == 1 ? CallDirection::Incoming : CallDirection::Outgoing;
    e.remoteUri = query.value(3).toString();
    e.remoteName = query.value(4).toString();
    e.startedAt = parseUtcIso(query.value(5).toString());
    if (!query.isNull(6)) {
        e.connectedAt = parseUtcIso(query.value(6).toString());
    }
    e.endedAt = parseUtcIso(query.value(7).toString());
    e.statusCode = query.value(8).toInt();
    e.missed = query.value(9).toInt() != 0;
    e.seen = query.value(10).toInt() != 0;
    return e;
}

} // namespace

HistoryRepository::HistoryRepository(Database &database) : database_(database) {}

qint64 HistoryRepository::insert(const HistoryEntry &entry) {
    QSqlQuery query(database_.connection());
    if (!query.prepare(QStringLiteral(
            "INSERT INTO history(account_id, direction, remote_uri, remote_name, started_at, "
            "connected_at, ended_at, status_code, missed, seen) "
            "VALUES (:account_id, :direction, :remote_uri, :remote_name, :started_at, "
            ":connected_at, :ended_at, :status_code, :missed, :seen)"))) {
        return -1;
    }

    query.bindValue(QStringLiteral(":account_id"), nonNull(entry.accountId));
    query.bindValue(QStringLiteral(":direction"),
                    entry.direction == CallDirection::Incoming ? 1 : 0);
    query.bindValue(QStringLiteral(":remote_uri"), nonNull(entry.remoteUri));
    query.bindValue(QStringLiteral(":remote_name"), nonNull(entry.remoteName));
    query.bindValue(QStringLiteral(":started_at"), formatUtcIso(entry.startedAt));
    if (entry.connectedAt.isValid()) {
        query.bindValue(QStringLiteral(":connected_at"), formatUtcIso(entry.connectedAt));
    } else {
        query.bindValue(QStringLiteral(":connected_at"), QVariant(QMetaType(QMetaType::QString)));
    }
    query.bindValue(QStringLiteral(":ended_at"), formatUtcIso(entry.endedAt));
    query.bindValue(QStringLiteral(":status_code"), entry.statusCode);
    query.bindValue(QStringLiteral(":missed"), entry.missed ? 1 : 0);
    query.bindValue(QStringLiteral(":seen"), entry.seen ? 1 : 0);

    if (!query.exec()) {
        return -1;
    }
    return query.lastInsertId().toLongLong();
}

bool HistoryRepository::remove(qint64 id) {
    QSqlQuery query(database_.connection());
    if (!query.prepare(QStringLiteral("DELETE FROM history WHERE id = :id"))) {
        return false;
    }
    query.bindValue(QStringLiteral(":id"), id);
    return query.exec() && query.numRowsAffected() > 0;
}

std::optional<HistoryEntry> HistoryRepository::findById(qint64 id) const {
    QSqlQuery query(database_.connection());
    if (!query.prepare(QStringLiteral(
            "SELECT id, account_id, direction, remote_uri, remote_name, started_at, connected_at, "
            "ended_at, status_code, missed, seen FROM history WHERE id = :id"))) {
        return std::nullopt;
    }
    query.bindValue(QStringLiteral(":id"), id);
    if (!query.exec() || !query.next()) {
        return std::nullopt;
    }
    return readHistoryRow(query);
}

QList<HistoryEntry> HistoryRepository::search(const QString &text, HistoryFilter filter,
                                              int limit) const {
    QSqlQuery query(database_.connection());
    if (!query.prepare(QStringLiteral(
            "SELECT id, account_id, direction, remote_uri, remote_name, started_at, connected_at, "
            "ended_at, status_code, missed, seen "
            "FROM history "
            "WHERE (:filter = 0 "
            "       OR (:filter = 1 AND missed = 1) "
            "       OR (:filter = 2 AND direction = 1) "
            "       OR (:filter = 3 AND direction = 0)) "
            "  AND (:pattern IS NULL OR :pattern = '' "
            "       OR remote_uri LIKE :pattern ESCAPE '\\' "
            "       OR remote_name LIKE :pattern ESCAPE '\\') "
            "ORDER BY started_at DESC, id DESC "
            "LIMIT :limit"))) {
        return {};
    }

    const int clampedLimit = limit > 0 ? limit : 200;
    query.bindValue(QStringLiteral(":filter"), static_cast<int>(filter));
    query.bindValue(QStringLiteral(":pattern"), buildLikePattern(text));
    query.bindValue(QStringLiteral(":limit"), clampedLimit);

    if (!query.exec()) {
        return {};
    }
    QList<HistoryEntry> results;
    while (query.next()) {
        results.append(readHistoryRow(query));
    }
    return results;
}

bool HistoryRepository::markAllSeen() {
    QSqlQuery query(database_.connection());
    if (!query.prepare(
            QStringLiteral("UPDATE history SET seen = 1 WHERE missed = 1 AND seen = 0"))) {
        return false;
    }
    return query.exec();
}

int HistoryRepository::unseenMissedCount() const {
    QSqlQuery query(database_.connection());
    if (!query.prepare(
            QStringLiteral("SELECT COUNT(*) FROM history WHERE missed = 1 AND seen = 0")) ||
        !query.exec() || !query.next()) {
        return 0;
    }
    return query.value(0).toInt();
}

bool HistoryRepository::clear() {
    QSqlQuery query(database_.connection());
    return query.prepare(QStringLiteral("DELETE FROM history")) && query.exec();
}

} // namespace tincan
