// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "app/database.h"

#include "app/app_paths.h"
#include "app/migrations.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

#include <cerrno>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

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

// History, contacts and messages are private. A missing directory is created
// 0700 (Tincan's own data directory is tightened too); the database file is
// created or tightened to 0600 before SQLite opens it, and SQLite gives its
// -wal and -shm files the same mode as the database file.
bool prepareOwnerOnlyFile(const QString &path, QString *error) {
    const QString dir = QFileInfo(path).absolutePath();
    const QByteArray nativeDir = QFile::encodeName(dir);
    if (!QFileInfo(dir).isDir() &&
        (!QDir().mkpath(QFileInfo(dir).absolutePath()) ||
         (::mkdir(nativeDir.constData(), S_IRWXU) != 0 && errno != EEXIST))) {
        setOutError(error, QStringLiteral("Cannot create %1").arg(dir));
        return false;
    }
    if (QDir(dir) == QDir(AppPaths::dataDir())) {
        (void)::chmod(nativeDir.constData(), S_IRWXU);
    }
    for (const QString &suffix : {QString(), QStringLiteral("-wal"), QStringLiteral("-shm")}) {
        const QByteArray native = QFile::encodeName(path + suffix);
        const int flags = O_RDWR | O_NOFOLLOW | O_CLOEXEC | (suffix.isEmpty() ? O_CREAT : 0);
        const int fd = ::open(native.constData(), flags, S_IRUSR | S_IWUSR);
        if (fd < 0) {
            if (suffix.isEmpty() || errno != ENOENT) {
                setOutError(error, QStringLiteral("Cannot open %1").arg(path + suffix));
                return false;
            }
            continue;
        }
        (void)::fchmod(fd, S_IRUSR | S_IWUSR);
        ::close(fd);
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
    if (dbPath_ != QStringLiteral(":memory:") && !prepareOwnerOnlyFile(dbPath_, error)) {
        return false;
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
