// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "app/contact_repository.h"
#include "app/database.h"
#include "app/history_repository.h"
#include "app/message_repository.h"

#include <QDateTime>
#include <QTest>

class TestRepositories : public QObject {
    Q_OBJECT

private slots:
    void contactRepositoryCrudAndSearch() {
        tincan::Database db(QStringLiteral(":memory:"));
        QVERIFY(db.open());
        tincan::ContactRepository repo(db);

        tincan::Contact alice;
        alice.name = QStringLiteral("Alice Vance");
        alice.number = QStringLiteral("204");
        alice.accountId = QStringLiteral("acc-1");
        alice.presence = true;
        alice.notes = QStringLiteral("Engineering");

        const qint64 aliceId = repo.insert(alice);
        QVERIFY(aliceId > 0);

        tincan::Contact bob;
        bob.name = QStringLiteral("Bob_100%");
        bob.number = QStringLiteral("305");
        const qint64 bobId = repo.insert(bob);
        QVERIFY(bobId > 0);

        const auto loaded = repo.findById(aliceId);
        QVERIFY(loaded.has_value());
        QCOMPARE(loaded->name, QStringLiteral("Alice Vance"));
        QCOMPARE(loaded->number, QStringLiteral("204"));
        QCOMPARE(loaded->presence, true);
        QCOMPARE(loaded->notes, QStringLiteral("Engineering"));

        alice.id = aliceId;
        alice.name = QStringLiteral("Alice V.");
        alice.presence = false;
        QVERIFY(repo.update(alice));
        QCOMPARE(repo.findById(aliceId)->name, QStringLiteral("Alice V."));
        QCOMPARE(repo.findById(aliceId)->presence, false);

        QCOMPARE(repo.listAll().size(), 2);
        QCOMPARE(repo.search(QStringLiteral("204")).size(), 1);
        // Literal % and _ in search text are escaped, not treated as SQL wildcards.
        QCOMPARE(repo.search(QStringLiteral("100%")).size(), 1);
        QCOMPARE(repo.search(QStringLiteral("100%")).first().id, bobId);

        QVERIFY(repo.remove(aliceId));
        QVERIFY(!repo.findById(aliceId).has_value());
        QCOMPARE(repo.listAll().size(), 1);
    }

    void historyRepositorySearchFiltersMissedCountsAndClear() {
        tincan::Database db(QStringLiteral(":memory:"));
        QVERIFY(db.open());
        tincan::HistoryRepository repo(db);

        const QDateTime base = QDateTime::fromString(QStringLiteral("2026-05-01T10:00:00.000Z"),
                                                     Qt::ISODateWithMs);

        tincan::HistoryEntry outConnected;
        outConnected.accountId = QStringLiteral("acc");
        outConnected.direction = tincan::CallDirection::Outgoing;
        outConnected.remoteUri = QStringLiteral("sip:201@pbx.example");
        outConnected.remoteName = QStringLiteral("Alice");
        outConnected.startedAt = base;
        outConnected.connectedAt = base.addSecs(3);
        outConnected.endedAt = base.addSecs(65);
        outConnected.statusCode = 200;
        outConnected.missed = false;
        outConnected.seen = true;
        QVERIFY(repo.insert(outConnected) > 0);

        tincan::HistoryEntry inAnswered;
        inAnswered.accountId = QStringLiteral("acc");
        inAnswered.direction = tincan::CallDirection::Incoming;
        inAnswered.remoteUri = QStringLiteral("sip:202@pbx.example");
        inAnswered.remoteName = QStringLiteral("Bob");
        inAnswered.startedAt = base.addSecs(100);
        inAnswered.connectedAt = base.addSecs(104);
        inAnswered.endedAt = base.addSecs(150);
        inAnswered.statusCode = 200;
        inAnswered.missed = false;
        inAnswered.seen = true;
        QVERIFY(repo.insert(inAnswered) > 0);

        tincan::HistoryEntry inMissed;
        inMissed.accountId = QStringLiteral("acc");
        inMissed.direction = tincan::CallDirection::Incoming;
        inMissed.remoteUri = QStringLiteral("sip:203@pbx.example");
        inMissed.remoteName = QStringLiteral("Carol");
        inMissed.startedAt = base.addSecs(200);
        inMissed.endedAt = base.addSecs(215);
        inMissed.statusCode = 487;
        inMissed.missed = true;
        inMissed.seen = false;
        const qint64 missedId = repo.insert(inMissed);
        QVERIFY(missedId > 0);
        QVERIFY(!repo.findById(missedId)->connectedAt.isValid());

        QCOMPARE(repo.search({}, tincan::HistoryFilter::All).size(), 3);
        QCOMPARE(repo.search({}, tincan::HistoryFilter::All).first().remoteName,
                 QStringLiteral("Carol"));
        QCOMPARE(repo.search({}, tincan::HistoryFilter::Missed).size(), 1);
        QCOMPARE(repo.search({}, tincan::HistoryFilter::In).size(), 2);
        QCOMPARE(repo.search({}, tincan::HistoryFilter::Out).size(), 1);
        QCOMPARE(repo.search(QStringLiteral("Bob"), tincan::HistoryFilter::All).size(), 1);
        QCOMPARE(repo.search({}, tincan::HistoryFilter::All, 2).size(), 2);

        QCOMPARE(repo.unseenMissedCount(), 1);
        QVERIFY(repo.markAllSeen());
        QCOMPARE(repo.unseenMissedCount(), 0);

        QVERIFY(repo.clear());
        QCOMPARE(repo.search().size(), 0);
    }

    void messageRepositoryStatusTransitionsConversationsAndUnread() {
        tincan::Database db(QStringLiteral(":memory:"));
        QVERIFY(db.open());
        tincan::MessageRepository repo(db);

        const QDateTime t0 = QDateTime::fromString(QStringLiteral("2026-05-01T12:00:00.000Z"),
                                                   Qt::ISODateWithMs);

        tincan::ChatMessage outMsg;
        outMsg.accountId = QStringLiteral("acc");
        outMsg.peerUri = QStringLiteral("sip:alice@pbx.example");
        outMsg.direction = tincan::MessageDirection::Outgoing;
        outMsg.body = QStringLiteral("Hello Alice");
        outMsg.sentAt = t0;
        outMsg.status = tincan::MessageStatus::Pending;
        outMsg.read = true;
        const qint64 outId = repo.insert(outMsg);
        QVERIFY(outId > 0);
        QCOMPARE(repo.findById(outId)->status, tincan::MessageStatus::Pending);

        QVERIFY(repo.updateStatus(outId, tincan::MessageStatus::Sent));
        QCOMPARE(repo.findById(outId)->status, tincan::MessageStatus::Sent);

        QVERIFY(repo.updateStatus(outId, tincan::MessageStatus::Failed));
        QCOMPARE(repo.findById(outId)->status, tincan::MessageStatus::Failed);

        tincan::ChatMessage in1;
        in1.accountId = QStringLiteral("acc");
        in1.peerUri = QStringLiteral("sip:alice@pbx.example");
        in1.direction = tincan::MessageDirection::Incoming;
        in1.body = QStringLiteral("Reply 1");
        in1.sentAt = t0.addSecs(10);
        in1.status = tincan::MessageStatus::Received;
        in1.read = false;
        QVERIFY(repo.insert(in1) > 0);

        tincan::ChatMessage in2 = in1;
        in2.peerUri = QStringLiteral("sip:bob@pbx.example");
        in2.body = QStringLiteral("Hey from Bob");
        in2.sentAt = t0.addSecs(20);
        QVERIFY(repo.insert(in2) > 0);

        QCOMPARE(repo.unreadCountForPeer(QStringLiteral("sip:alice@pbx.example")), 1);
        QCOMPARE(repo.totalUnreadCount(), 2);

        const auto convs = repo.listConversations();
        QCOMPARE(convs.size(), 2);
        QCOMPARE(convs.first().peerUri, QStringLiteral("sip:bob@pbx.example"));
        QCOMPARE(convs.first().unreadCount, 1);
        QCOMPARE(convs.last().lastBody, QStringLiteral("Reply 1"));

        QVERIFY(repo.markPeerRead(QStringLiteral("sip:alice@pbx.example")));
        QCOMPARE(repo.unreadCountForPeer(QStringLiteral("sip:alice@pbx.example")), 0);
        QCOMPARE(repo.totalUnreadCount(), 1);
        QCOMPARE(repo.listForPeer(QStringLiteral("sip:alice@pbx.example")).size(), 2);
    }

    // Values reach SQLite only as bound parameters, so SQL-looking text is
    // stored and found verbatim and the schema stays intact.
    void sqlLookingTextRoundTripsVerbatim() {
        tincan::Database db(QStringLiteral(":memory:"));
        QVERIFY(db.open());
        const QString evil = QStringLiteral("Robert'); DROP TABLE contacts;-- \" %_\\");

        tincan::ContactRepository contacts(db);
        tincan::Contact c;
        c.name = evil;
        c.number = QStringLiteral("1' OR '1'='1");
        c.notes = evil;
        const qint64 id = contacts.insert(c);
        QVERIFY(id > 0);
        QCOMPARE(contacts.findById(id)->name, evil);
        QCOMPARE(contacts.findById(id)->number, QStringLiteral("1' OR '1'='1"));
        QCOMPARE(contacts.search(evil).size(), 1);
        QCOMPARE(contacts.search(QStringLiteral("' OR 1=1 --")).size(), 0);

        tincan::HistoryRepository history(db);
        tincan::HistoryEntry h;
        h.accountId = QStringLiteral("acc");
        h.remoteUri = QStringLiteral("sip:x@h");
        h.remoteName = evil;
        h.startedAt = h.endedAt = QDateTime::currentDateTimeUtc();
        QVERIFY(history.insert(h) > 0);
        QCOMPARE(history.search(evil).first().remoteName, evil);

        tincan::MessageRepository messages(db);
        tincan::ChatMessage m;
        m.accountId = QStringLiteral("acc");
        m.peerUri = evil;
        m.body = evil;
        m.sentAt = QDateTime::currentDateTimeUtc();
        QVERIFY(messages.insert(m) > 0);
        QCOMPARE(messages.listForPeer(evil).first().body, evil);
        QCOMPARE(contacts.listAll().size(), 1);
    }
};

QTEST_GUILESS_MAIN(TestRepositories)
#include "tst_repositories.moc"
