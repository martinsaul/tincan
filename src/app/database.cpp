// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "app/database.h"

#include "app/app_paths.h"
#include "app/migrations.h"

#include <QDir>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

namespace tincan {

namespace {

QString resolveDbPath(const QString &path) {
    const QString trimmed = path.trimmed();
    if (!trimmed.isEmpty()) {
        return trimmed;
    }
    return QDir(AppPaths::dataDir()).filePath(QStringLiteral("tincan.sqlite"));
}

QString resolveConnectionName(const QString &name) {
    const QString trimmed = name.trimmed();
    if (!trimmed.isEmpty()) {
        return trimmed;
    }
    return QStringLiteral("tincan-db-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
}

void setOutError(QString *error, const QString &message) {
    if (error != nullptr) {
        *error = message;
    }
}

bool execPragma(const QSqlDatabase &db, const QString &pragmaSql, QString *error) {
    QSqlQuery query(db);
    if (!query.prepare(pragmaSql) || !query.exec()) {
        setOutError(error, query.lastError().text());
        return false;
    }
    return true;
}

} // namespace

Database::Database(QString dbPath, QString connectionName)
    : dbPath_(resolveDbPath(dbPath)), connectionName_(resolveConnectionName(connectionName)) {}

Database::~Database() {
    close();
}

bool Database::open(QString *error) {
    if (isOpen()) {
        return true;
    }
    if (dbPath_ != QStringLiteral(":memory:")) {
        QDir().mkpath(QFileInfo(dbPath_).absolutePath());
    }

    QSqlDatabase db = QSqlDatabase::contains(connectionName_)
                          ? QSqlDatabase::database(connectionName_, false)
                          : QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName_);
    db.setDatabaseName(dbPath_);
    if (!db.open()) {
        setOutError(error, db.lastError().text());
        return false;
    }

    if (!execPragma(db, QStringLiteral("PRAGMA foreign_keys = ON"), error) ||
        !execPragma(db, QStringLiteral("PRAGMA journal_mode = WAL"), error)) {
        return false;
    }

    return Migrations::apply(db, error);
}

void Database::close() {
    if (!QSqlDatabase::contains(connectionName_)) {
        return;
    }
    {
        QSqlDatabase db = QSqlDatabase::database(connectionName_, false);
        if (db.isOpen()) {
            db.close();
        }
    }
    QSqlDatabase::removeDatabase(connectionName_);
}

bool Database::isOpen() const {
    if (!QSqlDatabase::contains(connectionName_)) {
        return false;
    }
    return QSqlDatabase::database(connectionName_, false).isOpen();
}

const QString &Database::databasePath() const { return dbPath_; }

const QString &Database::connectionName() const { return connectionName_; }

QSqlDatabase Database::connection() const {
    return QSqlDatabase::database(connectionName_, false);
}

} // namespace tincan
