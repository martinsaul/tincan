// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "app/account_service.h"
#include "app/contact_repository.h"
#include "app/contact_service.h"
#include "app/database.h"
#include "app/secret_store.h"
#include "app/settings_store.h"
#include "app/sip_event_router.h"
#include "fake_sip_engine.h"

#include <QTemporaryDir>
#include <QTest>

class TestContactService : public QObject {
    Q_OBJECT

private slots:
    void subscribesPresenceAfterRegistrationAndUpdatesLiveState() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        tincan::SettingsStore settings(tempDir.filePath(QStringLiteral("tincan.ini")));
        tincan::MemorySecretStore secrets;
        tincan::SipEventRouter router;
        tincan::test::FakeSipEngine engine(&router);
        tincan::AccountService accounts(engine, settings, secrets);

        tincan::Database db(QStringLiteral(":memory:"));
        QVERIFY(db.open());
        tincan::ContactRepository repo(db);
        tincan::ContactService contacts(engine, accounts, repo);
        router.addListener(&contacts);

        tincan::AccountConfig acc;
        acc.id = QStringLiteral("acc-1");
        acc.user = QStringLiteral("100");
        acc.domain = QStringLiteral("pbx.example.com");
        acc.registerAccount = true;
        QVERIFY(accounts.addAccount(acc));

        tincan::Contact alice;
        alice.name = QStringLiteral("Alice Vance");
        alice.number = QStringLiteral("204");
        alice.presence = true;
        const qint64 aliceId = contacts.addContact(alice);
        QVERIFY(aliceId > 0);

        // Not registered yet -> no SUBSCRIBE sent.
        QVERIFY(engine.subscribedPresences.isEmpty());

        accounts.handleRegistrationChanged(tincan::RegistrationInfo{
            QStringLiteral("acc-1"), tincan::RegState::Registered, 200, QStringLiteral("OK")});
        QCOMPARE(engine.subscribedPresences.size(), 1);
        QCOMPARE(engine.subscribedPresences.first().uri,
                 QStringLiteral("sip:204@pbx.example.com"));

        engine.injectPresence(tincan::PresenceInfo{QStringLiteral("sip:204@pbx.example.com"),
                                                   tincan::PresenceState::Busy,
                                                   QStringLiteral("In meeting")});
        const auto loaded = contacts.contact(aliceId);
        QVERIFY(loaded.has_value());
        QCOMPARE(contacts.presenceForContact(*loaded).state, tincan::PresenceState::Busy);
        QCOMPARE(contacts.presenceForContact(*loaded).note, QStringLiteral("In meeting"));

        // Removing contact unsubscribes presence.
        QVERIFY(contacts.removeContact(aliceId));
        QCOMPARE(engine.unsubscribedPresences.size(), 1);
    }

    void resolvesDisplayNameAcrossNumbersUrisAndDialPrefixes() {
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
        acc.dialPrefix = QStringLiteral("9");
        QVERIFY(accounts.addAccount(acc));

        tincan::Database db(QStringLiteral(":memory:"));
        QVERIFY(db.open());
        tincan::ContactRepository repo(db);
        tincan::ContactService contacts(engine, accounts, repo);

        tincan::Contact c1;
        c1.name = QStringLiteral("Alice Vance");
        c1.number = QStringLiteral("204");
        QVERIFY(contacts.addContact(c1) > 0);

        tincan::Contact c2;
        c2.name = QStringLiteral("External Support");
        c2.number = QStringLiteral("+1 (555) 867-5309");
        QVERIFY(contacts.addContact(c2) > 0);

        QCOMPARE(contacts.resolveDisplayName(QStringLiteral("204")),
                 QStringLiteral("Alice Vance"));
        QCOMPARE(contacts.resolveDisplayName(QStringLiteral("sip:204@pbx.example.com")),
                 QStringLiteral("Alice Vance"));
        QCOMPARE(
            contacts.resolveDisplayName(
                QStringLiteral("\"204\" <sip:204@pbx.example.com:5060;transport=udp>")),
            QStringLiteral("Alice Vance"));
        QCOMPARE(contacts.resolveDisplayName(QStringLiteral("sip:9204@pbx.example.com")),
                 QStringLiteral("Alice Vance"));
        QCOMPARE(contacts.resolveDisplayName(QStringLiteral("+15558675309")),
                 QStringLiteral("External Support"));
        QVERIFY(contacts.resolveDisplayName(QStringLiteral("999")).isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestContactService)
#include "tst_contactservice.moc"
