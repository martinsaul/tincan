// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "app/account_service.h"
#include "app/call_service.h"
#include "app/secret_store.h"
#include "app/settings_store.h"
#include "fake_sip_engine.h"

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

class TestCallService : public QObject {
    Q_OBJECT

private slots:
    void dialUsesDefaultAccountAndNormalizedUri() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        tincan::SettingsStore settings(tempDir.filePath(QStringLiteral("tincan.ini")));
        tincan::MemorySecretStore secrets;
        tincan::test::FakeSipEngine engine;
        tincan::AccountService accounts(engine, settings, secrets);
        tincan::CallService calls(engine, accounts, settings);
        engine.setEvents(&calls);

        tincan::AccountConfig acc;
        acc.id = QStringLiteral("default-acc");
        acc.user = QStringLiteral("100");
        acc.domain = QStringLiteral("pbx.example.com");
        acc.dialPrefix = QStringLiteral("0");
        QVERIFY(accounts.addAccount(acc));

        const QString err = calls.dial(QStringLiteral(" (555) 12-34 "));
        QVERIFY2(err.isEmpty(), qPrintable(err));
        QCOMPARE(engine.dialedCalls.size(), 1);
        QCOMPARE(engine.dialedCalls.first().accountId, QStringLiteral("default-acc"));
        QCOMPARE(engine.dialedCalls.first().uri, QStringLiteral("sip:05551234@pbx.example.com"));
        QCOMPARE(calls.calls().size(), 1);
    }

    void incomingTonesCallWaitingAndRejectCodes() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        tincan::SettingsStore settings(tempDir.filePath(QStringLiteral("tincan.ini")));
        tincan::MemorySecretStore secrets;
        tincan::test::FakeSipEngine engine;
        tincan::AccountService accounts(engine, settings, secrets);
        tincan::CallService calls(engine, accounts, settings);
        engine.setEvents(&calls);

        tincan::AccountConfig acc;
        acc.id = QStringLiteral("acc");
        acc.user = QStringLiteral("100");
        acc.domain = QStringLiteral("pbx.example.com");
        QVERIFY(accounts.addAccount(acc));

        QSignalSpy incomingSpy(&calls, &tincan::CallService::incomingCall);
        const int firstId = engine.injectIncomingCall(
            QStringLiteral("acc"), QStringLiteral("sip:201@pbx.example.com"),
            QStringLiteral("Bob"));
        QCOMPARE(incomingSpy.count(), 1);
        QVERIFY(engine.currentTone.has_value());
        QCOMPARE(*engine.currentTone, tincan::Tone::Ringing);

        // Second incoming while first call is active starts CallWaiting and rejects with 486.
        const int secondId = engine.injectIncomingCall(
            QStringLiteral("acc"), QStringLiteral("sip:202@pbx.example.com"),
            QStringLiteral("Carol"));
        QCOMPARE(incomingSpy.count(), 2);
        QVERIFY(engine.currentTone.has_value());
        QCOMPARE(*engine.currentTone, tincan::Tone::CallWaiting);

        QVERIFY(calls.reject(secondId));
        QCOMPARE(engine.rejectedCalls.size(), 1);
        QCOMPARE(engine.rejectedCalls.last().callId, secondId);
        QCOMPARE(engine.rejectedCalls.last().statusCode, 486);
        engine.injectCallState(secondId, tincan::CallState::Disconnected, 486);

        // Only first incoming remains -> Ringing; rejecting sole call uses 603 and stops tones.
        QVERIFY(engine.currentTone.has_value());
        QCOMPARE(*engine.currentTone, tincan::Tone::Ringing);
        QVERIFY(calls.reject(firstId));
        QCOMPARE(engine.rejectedCalls.last().statusCode, 603);
        engine.injectCallState(firstId, tincan::CallState::Disconnected, 603);
        QVERIFY(!engine.currentTone.has_value());
        QVERIFY(calls.calls().isEmpty());
    }

    void outgoingRingbackOnEarlyStopsOnConfirmedAndDisconnect() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        tincan::SettingsStore settings(tempDir.filePath(QStringLiteral("tincan.ini")));
        tincan::MemorySecretStore secrets;
        tincan::test::FakeSipEngine engine;
        tincan::AccountService accounts(engine, settings, secrets);
        tincan::CallService calls(engine, accounts, settings);
        engine.setEvents(&calls);

        tincan::AccountConfig acc;
        acc.id = QStringLiteral("acc");
        acc.user = QStringLiteral("100");
        acc.domain = QStringLiteral("pbx.example.com");
        QVERIFY(accounts.addAccount(acc));

        QVERIFY(calls.dial(QStringLiteral("300")).isEmpty());
        const int callId = engine.dialedCalls.first().callId;
        QVERIFY(!engine.currentTone.has_value());

        engine.injectCallState(callId, tincan::CallState::Early, 180, QStringLiteral("Ringing"));
        QVERIFY(engine.currentTone.has_value());
        QCOMPARE(*engine.currentTone, tincan::Tone::Ringback);

        engine.injectCallState(callId, tincan::CallState::Confirmed, 200, QStringLiteral("OK"));
        QVERIFY(!engine.currentTone.has_value());

        // Incoming call during Confirmed call also triggers CallWaiting and 486 on reject.
        const int incDuringActive = engine.injectIncomingCall(
            QStringLiteral("acc"), QStringLiteral("sip:400@pbx.example.com"));
        QVERIFY(engine.currentTone.has_value());
        QCOMPARE(*engine.currentTone, tincan::Tone::CallWaiting);
        QVERIFY(calls.reject(incDuringActive));
        QCOMPARE(engine.rejectedCalls.last().statusCode, 486);
        engine.injectCallState(incDuringActive, tincan::CallState::Disconnected, 486);
        QVERIFY(!engine.currentTone.has_value());

        engine.injectCallState(callId, tincan::CallState::Disconnected, 200);
        QVERIFY(!engine.currentTone.has_value());
        QVERIFY(calls.calls().isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestCallService)
#include "tst_callservice.moc"
