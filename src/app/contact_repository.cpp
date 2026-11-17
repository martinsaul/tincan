// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "app/contact_repository.h"

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

Contact readContactRow(const QSqlQuery &query) {
    Contact c;
    c.id = query.value(0).toLongLong();
    c.name = query.value(1).toString();
    c.number = query.value(2).toString();
    c.accountId = query.value(3).toString();
    c.presence = query.value(4).toInt() != 0;
    c.notes = query.value(5).toString();
    c.createdAt = parseUtcIso(query.value(6).toString());
    c.updatedAt = parseUtcIso(query.value(7).toString());
    return c;
}

} // namespace

ContactRepository::ContactRepository(Database &database) : database_(database) {}

qint64 ContactRepository::insert(const Contact &contact) {
    QSqlQuery query(database_.connection());
    if (!query.prepare(QStringLiteral(
            "INSERT INTO contacts(name, number, account_id, presence, notes, created_at, updated_at) "
            "VALUES (:name, :number, :account_id, :presence, :notes, :created_at, :updated_at)"))) {
        return -1;
    }

    const QDateTime now = QDateTime::currentDateTimeUtc();
    const QString createdIso = formatUtcIso(contact.createdAt.isValid() ? contact.createdAt : now);
    const QString updatedIso = formatUtcIso(contact.updatedAt.isValid() ? contact.updatedAt : now);

    query.bindValue(QStringLiteral(":name"), nonNull(contact.name));
    query.bindValue(QStringLiteral(":number"), nonNull(contact.number));
    query.bindValue(QStringLiteral(":account_id"), nonNull(contact.accountId));
    query.bindValue(QStringLiteral(":presence"), contact.presence ? 1 : 0);
    query.bindValue(QStringLiteral(":notes"), nonNull(contact.notes));
    query.bindValue(QStringLiteral(":created_at"), createdIso);
    query.bindValue(QStringLiteral(":updated_at"), updatedIso);

    if (!query.exec()) {
        return -1;
    }
    return query.lastInsertId().toLongLong();
}

bool ContactRepository::update(const Contact &contact) {
    if (contact.id <= 0) {
        return false;
    }
    QSqlQuery query(database_.connection());
    if (!query.prepare(QStringLiteral(
            "UPDATE contacts SET name = :name, number = :number, account_id = :account_id, "
            "presence = :presence, notes = :notes, updated_at = :updated_at WHERE id = :id"))) {
        return false;
    }

    query.bindValue(QStringLiteral(":name"), nonNull(contact.name));
    query.bindValue(QStringLiteral(":number"), nonNull(contact.number));
    query.bindValue(QStringLiteral(":account_id"), nonNull(contact.accountId));
    query.bindValue(QStringLiteral(":presence"), contact.presence ? 1 : 0);
    query.bindValue(QStringLiteral(":notes"), nonNull(contact.notes));
    query.bindValue(QStringLiteral(":updated_at"), formatUtcIso(contact.updatedAt));
    query.bindValue(QStringLiteral(":id"), contact.id);

    return query.exec() && query.numRowsAffected() > 0;
}

bool ContactRepository::remove(qint64 id) {
    QSqlQuery query(database_.connection());
    if (!query.prepare(QStringLiteral("DELETE FROM contacts WHERE id = :id"))) {
        return false;
    }
    query.bindValue(QStringLiteral(":id"), id);
    return query.exec() && query.numRowsAffected() > 0;
}

std::optional<Contact> ContactRepository::findById(qint64 id) const {
    QSqlQuery query(database_.connection());
    if (!query.prepare(QStringLiteral(
            "SELECT id, name, number, account_id, presence, notes, created_at, updated_at "
            "FROM contacts WHERE id = :id"))) {
        return std::nullopt;
    }
    query.bindValue(QStringLiteral(":id"), id);
    if (!query.exec() || !query.next()) {
        return std::nullopt;
    }
    return readContactRow(query);
}

QList<Contact> ContactRepository::listAll() const {
    return search({});
}

QList<Contact> ContactRepository::search(const QString &queryText) const {
    QSqlQuery query(database_.connection());
    if (!query.prepare(QStringLiteral(
            "SELECT id, name, number, account_id, presence, notes, created_at, updated_at "
            "FROM contacts "
            "WHERE (:pattern IS NULL OR :pattern = '' "
            "       OR name LIKE :pattern ESCAPE '\\' "
            "       OR number LIKE :pattern ESCAPE '\\') "
            "ORDER BY name COLLATE NOCASE ASC, id ASC"))) {
        return {};
    }
    query.bindValue(QStringLiteral(":pattern"), buildLikePattern(queryText));
    if (!query.exec()) {
        return {};
    }
    QList<Contact> results;
    while (query.next()) {
        results.append(readContactRow(query));
    }
    return results;
}

} // namespace tincan
