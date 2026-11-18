// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "app/account_service.h"
#include "app/secret_store.h"
#include "app/settings_store.h"
#include "fake_sip_engine.h"

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

class TestAccountService : public QObject {
    Q_OBJECT

private slots:
    void loadsAccountsFetchesPasswordsAndRegistersOnEngine() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        tincan::SettingsStore settings(tempDir.filePath(QStringLiteral("tincan.ini")));
        tincan::MemorySecretStore secrets;
        tincan::test::FakeSipEngine engine;

        tincan::AccountConfig stored;
        stored.id = QStringLiteral("office");
        stored.user = QStringLiteral("200");
        stored.domain = QStringLiteral("pbx.local");
        settings.setAccounts({stored});
        settings.setDefaultAccountId(stored.id);
        secrets.writePassword(stored.id, QStringLiteral("s3cr3t"));

        tincan::AccountService service(engine, settings, secrets);
        service.load();

        QCOMPARE(service.defaultAccountId(), QStringLiteral("office"));
        QVERIFY(engine.activeAccounts.contains(QStringLiteral("office")));
        QCOMPARE(engine.activeAccounts.value(QStringLiteral("office")).password,
                 QStringLiteral("s3cr3t"));
    }

    void addUpdateRemovePersistsAndUpdatesEngineAndRegistration() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        tincan::SettingsStore settings(tempDir.filePath(QStringLiteral("tincan.ini")));
        tincan::MemorySecretStore secrets;
        tincan::test::FakeSipEngine engine;
        tincan::AccountService service(engine, settings, secrets);

        tincan::AccountConfig acc;
        acc.id = QStringLiteral("main");
        acc.user = QStringLiteral("alice");
        acc.domain = QStringLiteral("sip.test");
        acc.password = QStringLiteral("pw1");

        QVERIFY(service.addAccount(acc));
        QCOMPARE(service.defaultAccountId(), QStringLiteral("main"));
        QCOMPARE(secrets.peekPassword(QStringLiteral("main")), QStringLiteral("pw1"));
        QCOMPARE(settings.accounts().size(), 1);
        QVERIFY(settings.accounts().first().password.isEmpty());
        QVERIFY(engine.activeAccounts.contains(QStringLiteral("main")));

        QSignalSpy regSpy(&service, &tincan::AccountService::registrationChanged);
        service.handleRegistrationChanged(tincan::RegistrationInfo{
            QStringLiteral("main"), tincan::RegState::Registered, 200, QStringLiteral("OK")});
        QCOMPARE(regSpy.count(), 1);
        QCOMPARE(service.registrationInfo(QStringLiteral("main")).state,
                 tincan::RegState::Registered);

        acc.password = QStringLiteral("pw2");
        acc.displayName = QStringLiteral("Alice Updated");
        QVERIFY(service.updateAccount(acc));
        QCOMPARE(secrets.peekPassword(QStringLiteral("main")), QStringLiteral("pw2"));
        QCOMPARE(engine.activeAccounts.value(QStringLiteral("main")).password,
                 QStringLiteral("pw2"));
        QCOMPARE(settings.accounts().first().displayName, QStringLiteral("Alice Updated"));

        service.removeAccount(QStringLiteral("main"));
        QVERIFY(!engine.activeAccounts.contains(QStringLiteral("main")));
        QVERIFY(!secrets.contains(QStringLiteral("main")));
        QVERIFY(settings.accounts().isEmpty());
        QVERIFY(service.defaultAccountId().isEmpty());
    }
    void updateWithEmptyPasswordKeepsStoredSecret() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        tincan::SettingsStore settings(tempDir.filePath(QStringLiteral("tincan.ini")));
        tincan::MemorySecretStore secrets;
        tincan::test::FakeSipEngine engine;
        tincan::AccountService service(engine, settings, secrets);

        tincan::AccountConfig acc;
        acc.id = QStringLiteral("main");
        acc.user = QStringLiteral("alice");
        acc.domain = QStringLiteral("sip.test");
        acc.password = QStringLiteral("pw1");
        QVERIFY(service.addAccount(acc));

        acc.password.clear();
        acc.displayName = QStringLiteral("Renamed");
        QVERIFY(service.updateAccount(acc));
        QCOMPARE(secrets.peekPassword(QStringLiteral("main")), QStringLiteral("pw1"));
        QCOMPARE(engine.activeAccounts.value(QStringLiteral("main")).password,
                 QStringLiteral("pw1"));
    }

    void refusesChangesWhileAccountHasCalls() {
        QTemporaryDir tempDir;
        tincan::SettingsStore settings(tempDir.filePath(QStringLiteral("tincan.ini")));
        tincan::MemorySecretStore secrets;
        tincan::test::FakeSipEngine engine;
        tincan::AccountService service(engine, settings, secrets);
        tincan::AccountConfig acc;
        acc.id = QStringLiteral("busy");
        acc.domain = QStringLiteral("sip.test");
        QVERIFY(service.addAccount(acc));
        tincan::AccountConfig other = acc;
        other.id = QStringLiteral("idle");
        QVERIFY(service.addAccount(other));

        const int callId = engine.makeCall(acc.id, QStringLiteral("sip:100@sip.test"), nullptr);
        QVERIFY(callId >= 0);
        QVERIFY(service.hasActiveCalls(acc.id));
        QVERIFY(!service.hasActiveCalls(other.id));

        acc.displayName = QStringLiteral("Renamed");
        QString error;
        QVERIFY(!service.updateAccount(acc, &error));
        QVERIFY(!error.isEmpty());
        error.clear();
        QVERIFY(!service.removeAccount(acc.id, &error));
        QVERIFY(!error.isEmpty());
        QVERIFY(service.account(acc.id).has_value());
        QVERIFY(engine.activeAccounts.contains(acc.id));
        // Accounts without calls stay editable.
        QVERIFY(service.removeAccount(other.id));

        engine.injectCallState(callId, tincan::CallState::Disconnected);
        QVERIFY(!service.hasActiveCalls(acc.id));
        QVERIFY(service.updateAccount(acc));
        QVERIFY(service.removeAccount(acc.id));
        QVERIFY(!service.account(acc.id).has_value());
    }
};

QTEST_GUILESS_MAIN(TestAccountService)
#include "tst_accountservice.moc"
