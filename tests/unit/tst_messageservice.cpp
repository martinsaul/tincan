// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "app/account_service.h"
#include "app/database.h"
#include "app/message_repository.h"
#include "app/message_service.h"
#include "app/secret_store.h"
#include "app/settings_store.h"
#include "fake_sip_engine.h"

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

class TestMessageService : public QObject {
    Q_OBJECT

private slots:
    void sendReceiveStatusTransitionsAndUnreadCounts() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        tincan::SettingsStore settings(tempDir.filePath(QStringLiteral("tincan.ini")));
        tincan::MemorySecretStore secrets;
        tincan::test::FakeSipEngine engine;
        tincan::AccountService accounts(engine, settings, secrets);

        tincan::AccountConfig acc;
        acc.id = QStringLiteral("acc-1");
        acc.user = QStringLiteral("100");
        acc.domain = QStringLiteral("pbx.example.com");
        QVERIFY(accounts.addAccount(acc));

        tincan::Database db(QStringLiteral(":memory:"));
        QVERIFY(db.open());
        tincan::MessageRepository repo(db);
        tincan::MessageService messages(engine, accounts, repo);
        engine.setEvents(&messages);

        // Outgoing message -> Pending -> Sent on 200 OK.
        const qint64 m1 = messages.sendMessage(QStringLiteral("204"), QStringLiteral("Hello!"));
        QVERIFY(m1 > 0);
        QCOMPARE(engine.sentMessages.size(), 1);
        QCOMPARE(engine.sentMessages.first().uri, QStringLiteral("sip:204@pbx.example.com"));
        QCOMPARE(repo.findById(m1)->status, tincan::MessageStatus::Pending);

        const quint64 tok1 = engine.sentMessages.first().token;
        engine.injectMessageStatus(tok1, 200, QStringLiteral("OK"));
        QCOMPARE(repo.findById(m1)->status, tincan::MessageStatus::Sent);

        // Outgoing message -> Pending -> Failed on 404.
        const qint64 m2 = messages.sendMessage(QStringLiteral("204"), QStringLiteral("Ping?"));
        QVERIFY(m2 > 0);
        const quint64 tok2 = engine.sentMessages.last().token;
        engine.injectMessageStatus(tok2, 404, QStringLiteral("Not Found"));
        QCOMPARE(repo.findById(m2)->status, tincan::MessageStatus::Failed);

        // Incoming message -> unread increments, markPeerRead clears.
        QSignalSpy incomingSpy(&messages, &tincan::MessageService::incomingMessageReceived);
        QSignalSpy unreadSpy(&messages, &tincan::MessageService::unreadCountChanged);
        engine.injectIncomingMessage(tincan::MessageInfo{
            QStringLiteral("acc-1"),
            QStringLiteral("\"Alice\" <sip:204@pbx.example.com>"),
            QStringLiteral("Alice"),
            QStringLiteral("Hi back!"),
            QStringLiteral("text/plain"),
            QDateTime::currentDateTimeUtc(),
        });

        QCOMPARE(incomingSpy.count(), 1);
        QCOMPARE(messages.unreadCountForPeer(QStringLiteral("204")), 1);
        QCOMPARE(messages.totalUnreadCount(), 1);
        QCOMPARE(unreadSpy.last().first().toInt(), 1);

        messages.markPeerRead(QStringLiteral("sip:204@pbx.example.com"));
        QCOMPARE(messages.unreadCountForPeer(QStringLiteral("204")), 0);
        QCOMPARE(messages.totalUnreadCount(), 0);
        QCOMPARE(unreadSpy.last().first().toInt(), 0);
        QCOMPARE(messages.messagesForPeer(QStringLiteral("204")).size(), 3);
    }
};

QTEST_GUILESS_MAIN(TestMessageService)
#include "tst_messageservice.moc"
