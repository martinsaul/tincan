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

    void autoHoldsActiveCallOnSecondDialOrAnswerUnlessConferenced() {
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

        QVERIFY(calls.dial(QStringLiteral("201")).isEmpty());
        const int firstId = engine.dialedCalls.first().callId;
        engine.injectCallState(firstId, tincan::CallState::Confirmed, 200);

        // Dialing a second call holds the first Confirmed call.
        QVERIFY(calls.dial(QStringLiteral("202")).isEmpty());
        const int secondId = engine.dialedCalls.last().callId;
        QCOMPARE(engine.holdOperations.size(), 1);
        QCOMPARE(engine.holdOperations.last().callId, firstId);
        QCOMPARE(engine.holdOperations.last().hold, true);
        engine.injectCallState(secondId, tincan::CallState::Confirmed, 200);

        // Merging both into a conference unholds and prevents auto-hold on third incoming answer.
        QVERIFY(calls.mergeAllCalls());
        const int thirdId =
            engine.injectIncomingCall(QStringLiteral("acc"), QStringLiteral("sip:203@pbx.example.com"));
        const qsizetype holdsBeforeAnswer = engine.holdOperations.size();
        QVERIFY(calls.answer(thirdId));
        QCOMPARE(engine.holdOperations.size(), holdsBeforeAnswer);
    }

    void dndRejectsIncomingWith486WithoutRinging() {
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

        QSignalSpy dndSpy(&calls, &tincan::CallService::dndChanged);
        calls.setDnd(true);
        QCOMPARE(dndSpy.count(), 1);
        QVERIFY(calls.isDnd());

        QSignalSpy incomingSpy(&calls, &tincan::CallService::incomingCall);
        const int incId = engine.injectIncomingCall(QStringLiteral("acc"),
                                                    QStringLiteral("sip:500@pbx.example.com"));
        QCOMPARE(incomingSpy.count(), 0);
        QVERIFY(!engine.currentTone.has_value());
        QCOMPARE(engine.rejectedCalls.size(), 1);
        QCOMPARE(engine.rejectedCalls.first().callId, incId);
        QCOMPARE(engine.rejectedCalls.first().statusCode, 486);
    }

    void forwardOnNoAnswerRedirectsAfterShortTimer() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        tincan::SettingsStore settings(tempDir.filePath(QStringLiteral("tincan.ini")));
        tincan::CallSettings cs = settings.callSettings();
        cs.forwardOnNoAnswer = QStringLiteral("305");
        cs.forwardNoAnswerSec = 1;
        settings.setCallSettings(cs);

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

        const int incId = engine.injectIncomingCall(QStringLiteral("acc"),
                                                    QStringLiteral("sip:401@pbx.example.com"));
        QVERIFY(engine.redirectedCalls.isEmpty());

        QTRY_COMPARE_WITH_TIMEOUT(engine.redirectedCalls.size(), 1, 2500);
        QCOMPARE(engine.redirectedCalls.first().callId, incId);
        QCOMPARE(engine.redirectedCalls.first().uri, QStringLiteral("sip:305@pbx.example.com"));
    }

    void autoAnswerWithDelayAndAutoRecordWavNaming() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString recDir = tempDir.filePath(QStringLiteral("recordings"));
        tincan::SettingsStore settings(tempDir.filePath(QStringLiteral("tincan.ini")));
        tincan::CallSettings cs = settings.callSettings();
        cs.autoAnswerMode = tincan::AutoAnswerMode::HeaderOnly;
        cs.autoAnswerDelaySec = 5;
        cs.recordingsDir = recDir;
        cs.autoRecord = true;
        cs.dtmfMethod = tincan::DtmfMethod::SipInfo;
        settings.setCallSettings(cs);

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

        const int incId = engine.injectIncomingCall(
            QStringLiteral("acc"), QStringLiteral("sip:alice@pbx.example.com"), QStringLiteral("Alice"),
            {{QStringLiteral("Call-Info"), QStringLiteral("<sip:pbx>;answer-after=1")}});
        QVERIFY(engine.answeredCallIds.isEmpty());

        QTRY_COMPARE_WITH_TIMEOUT(engine.answeredCallIds.size(), 1, 2500);
        QCOMPARE(engine.answeredCallIds.first(), incId);

        engine.injectCallState(incId, tincan::CallState::Confirmed, 200);
        QCOMPARE(engine.startedRecordings.size(), 1);
        QVERIFY(engine.startedRecordings.first().wavPath.startsWith(recDir));
        QVERIFY(engine.startedRecordings.first().wavPath.endsWith(QStringLiteral("-alice.wav")));

        QVERIFY(calls.sendDtmfToActive(QStringLiteral("9#")));
        QCOMPARE(engine.sentDtmf.size(), 1);
        QCOMPARE(engine.sentDtmf.first().digits, QStringLiteral("9#"));
        QCOMPARE(engine.sentDtmf.first().method, tincan::DtmfMethod::SipInfo);
    }
};

QTEST_GUILESS_MAIN(TestCallService)
#include "tst_callservice.moc"
