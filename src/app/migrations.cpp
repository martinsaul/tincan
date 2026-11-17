// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "app/migrations.h"

#include <QDateTime>
#include <QList>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>

namespace tincan::Migrations {

namespace {

struct MigrationStep {
    int version = 0;
    QStringList statements;
};

void setOutError(QString *error, const QString &message) {
    if (error != nullptr) {
        *error = message;
    }
}

QList<MigrationStep> allSteps() {
    MigrationStep v1;
    v1.version = 1;
    v1.statements = {
        QStringLiteral("CREATE TABLE contacts ("
                       "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                       "name TEXT NOT NULL, "
                       "number TEXT NOT NULL, "
                       "account_id TEXT, "
                       "presence INTEGER NOT NULL DEFAULT 0, "
                       "notes TEXT, "
                       "created_at TEXT NOT NULL, "
                       "updated_at TEXT NOT NULL"
                       ")"),
        QStringLiteral("CREATE TABLE history ("
                       "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                       "account_id TEXT NOT NULL, "
                       "direction INTEGER NOT NULL, "
                       "remote_uri TEXT NOT NULL, "
                       "remote_name TEXT NOT NULL DEFAULT '', "
                       "started_at TEXT NOT NULL, "
                       "connected_at TEXT NULL, "
                       "ended_at TEXT NOT NULL, "
                       "status_code INTEGER NOT NULL DEFAULT 0, "
                       "missed INTEGER NOT NULL DEFAULT 0, "
                       "seen INTEGER NOT NULL DEFAULT 0"
                       ")"),
        QStringLiteral("CREATE TABLE messages ("
                       "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                       "account_id TEXT NOT NULL, "
                       "peer_uri TEXT NOT NULL, "
                       "direction INTEGER NOT NULL, "
                       "body TEXT NOT NULL, "
                       "sent_at TEXT NOT NULL, "
                       "status INTEGER NOT NULL DEFAULT 0, "
                       "read INTEGER NOT NULL DEFAULT 0"
                       ")"),
        QStringLiteral("CREATE INDEX idx_history_started_at ON history(started_at)"),
        QStringLiteral("CREATE INDEX idx_messages_peer_uri_sent_at ON messages(peer_uri, sent_at)"),
    };
    return {v1};
}

bool ensureVersionTable(const QSqlDatabase &db, QString *error) {
    QSqlQuery query(db);
    if (!query.prepare(QStringLiteral("CREATE TABLE IF NOT EXISTS schema_version ("
                                      "version INTEGER PRIMARY KEY, "
                                      "applied_at TEXT NOT NULL"
                                      ")")) ||
        !query.exec()) {
        setOutError(error, query.lastError().text());
        return false;
    }
    return true;
}

bool applySingleStep(QSqlDatabase &db, const MigrationStep &step, QString *error) {
    if (!db.transaction()) {
        setOutError(error, db.lastError().text());
        return false;
    }

    for (const QString &sql : step.statements) {
        QSqlQuery stmt(db);
        if (!stmt.prepare(sql) || !stmt.exec()) {
            setOutError(error, stmt.lastError().text());
            (void)db.rollback();
            return false;
        }
    }

    QSqlQuery rec(db);
    if (!rec.prepare(QStringLiteral(
            "INSERT INTO schema_version(version, applied_at) VALUES (:version, :applied_at)"))) {
        setOutError(error, rec.lastError().text());
        (void)db.rollback();
        return false;
    }
    rec.bindValue(QStringLiteral(":version"), step.version);
    rec.bindValue(QStringLiteral(":applied_at"),
                  QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    if (!rec.exec()) {
        setOutError(error, rec.lastError().text());
        (void)db.rollback();
        return false;
    }

    if (!db.commit()) {
        setOutError(error, db.lastError().text());
        (void)db.rollback();
        return false;
    }
    return true;
}

} // namespace

int latestVersion() {
    const QList<MigrationStep> steps = allSteps();
    return steps.isEmpty() ? 0 : steps.last().version;
}

int currentVersion(const QSqlDatabase &db) {
    QSqlQuery query(db);
    if (!query.prepare(QStringLiteral("SELECT COALESCE(MAX(version), 0) FROM schema_version")) ||
        !query.exec() || !query.next()) {
        return 0;
    }
    return query.value(0).toInt();
}

bool apply(QSqlDatabase db, QString *error) {
    if (!db.isOpen()) {
        setOutError(error, QStringLiteral("Database is not open"));
        return false;
    }
    if (!ensureVersionTable(db, error)) {
        return false;
    }

    const int current = currentVersion(db);
    for (const MigrationStep &step : allSteps()) {
        if (step.version <= current) {
            continue;
        }
        if (!applySingleStep(db, step, error)) {
            return false;
        }
    }
    return true;
}

} // namespace tincan::Migrations
