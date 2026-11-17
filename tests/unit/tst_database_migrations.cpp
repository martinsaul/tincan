// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "app/database.h"
#include "app/migrations.h"

#include <QSet>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTest>

namespace {

QSet<QString> sqliteObjectsOfType(const QSqlDatabase &db, const QString &type) {
    QSet<QString> names;
    QSqlQuery q(db);
    if (q.prepare(QStringLiteral("SELECT name FROM sqlite_master WHERE type = :type"))) {
        q.bindValue(QStringLiteral(":type"), type);
        if (q.exec()) {
            while (q.next()) {
                names.insert(q.value(0).toString());
            }
        }
    }
    return names;
}

} // namespace

class TestDatabaseMigrations : public QObject {
    Q_OBJECT

private slots:
    void freshInMemoryDbCreatesTablesIndexesAndPragmas() {
        tincan::Database db(QStringLiteral(":memory:"));
        QString error;
        QVERIFY2(db.open(&error), qPrintable(error));
        QVERIFY(db.isOpen());
        QCOMPARE(tincan::Migrations::currentVersion(db.connection()),
                 tincan::Migrations::latestVersion());

        const QSet<QString> tables = sqliteObjectsOfType(db.connection(), QStringLiteral("table"));
        QVERIFY(tables.contains(QStringLiteral("schema_version")));
        QVERIFY(tables.contains(QStringLiteral("contacts")));
        QVERIFY(tables.contains(QStringLiteral("history")));
        QVERIFY(tables.contains(QStringLiteral("messages")));

        const QSet<QString> indexes = sqliteObjectsOfType(db.connection(), QStringLiteral("index"));
        QVERIFY(indexes.contains(QStringLiteral("idx_history_started_at")));
        QVERIFY(indexes.contains(QStringLiteral("idx_messages_peer_uri_sent_at")));

        QSqlQuery fkQuery(db.connection());
        QVERIFY(fkQuery.exec(QStringLiteral("PRAGMA foreign_keys")));
        QVERIFY(fkQuery.next());
        QCOMPARE(fkQuery.value(0).toInt(), 1);
    }

    void rerunMigrationsIsIdempotentAndFileDbUsesWal() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString dbFile = tempDir.filePath(QStringLiteral("test_tincan.sqlite"));

        tincan::Database db(dbFile);
        QString error;
        QVERIFY2(db.open(&error), qPrintable(error));

        QSqlQuery walQuery(db.connection());
        QVERIFY(walQuery.exec(QStringLiteral("PRAGMA journal_mode")));
        QVERIFY(walQuery.next());
        QCOMPARE(walQuery.value(0).toString().toLower(), QStringLiteral("wal"));

        QSqlQuery ins(db.connection());
        QVERIFY(ins.exec(QStringLiteral(
            "INSERT INTO contacts(name, number, presence, created_at, updated_at) "
            "VALUES ('Alice', '204', 1, '2026-01-01T00:00:00Z', '2026-01-01T00:00:00Z')")));

        QVERIFY2(tincan::Migrations::apply(db.connection(), &error), qPrintable(error));
        QVERIFY2(tincan::Migrations::apply(db.connection(), &error), qPrintable(error));
        QCOMPARE(tincan::Migrations::currentVersion(db.connection()),
                 tincan::Migrations::latestVersion());

        QSqlQuery countQuery(db.connection());
        QVERIFY(countQuery.exec(QStringLiteral("SELECT COUNT(*) FROM contacts")));
        QVERIFY(countQuery.next());
        QCOMPARE(countQuery.value(0).toInt(), 1);
    }
};

QTEST_GUILESS_MAIN(TestDatabaseMigrations)
#include "tst_database_migrations.moc"
