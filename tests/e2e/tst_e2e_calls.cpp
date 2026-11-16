// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "core/account_config.h"
#include "core/engine_config.h"
#include "pjsua_peer.h"
#include "recording_events.h"
#include "sip/pjsip_engine.h"

#include <QSignalSpy>
#include <QTest>

#include <memory>

class TestE2eCalls : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() {
        events_ = std::make_unique<tincan::test::RecordingEvents>();
        engine_ = std::make_unique<tincan::sip::PjsipEngine>(*events_);

        tincan::EngineConfig cfg;
        cfg.udpPort = 0;
        cfg.tcpPort = 0;
        cfg.enableTcp = false;
        cfg.nullAudio = true;
        cfg.userAgent = QStringLiteral("Tincan-E2E/0.1");

        QString error;
        QVERIFY2(engine_->start(cfg, &error), qPrintable(error));
        QVERIFY(engine_->localUdpPort() > 0);

        tincan::AccountConfig localAcc;
        localAcc.id = QStringLiteral("local");
        localAcc.user = QStringLiteral("tincan");
        localAcc.domain = QStringLiteral("127.0.0.1");
        localAcc.registerAccount = false;
        QVERIFY2(engine_->addAccount(localAcc, &error), qPrintable(error));
    }

    void cleanup() {
        for (const tincan::CallInfo &call : engine_->calls()) {
            engine_->hangup(call.callId);
        }
        QTRY_VERIFY_WITH_TIMEOUT(engine_->calls().isEmpty(), 5000);
        events_->clear();
    }

    void cleanupTestCase() {
        if (engine_) {
            engine_->shutdown();
            engine_.reset();
        }
        events_.reset();
    }

    void outgoingCallConnectsAndHangsUp() {
        tincan::test::PjsuaPeer peer;
        QVERIFY2(peer.start(200, 30), qPrintable(peer.output()));

        QString error;
        const int callId = engine_->makeCall(QStringLiteral("local"), peer.sipUri(), &error);
        QVERIFY2(callId >= 0, qPrintable(error));

        QTRY_VERIFY_WITH_TIMEOUT(
            events_->hasCallState(callId, tincan::CallState::Confirmed), 10000);
        const auto confirmed = events_->findCallState(callId, tincan::CallState::Confirmed);
        QVERIFY(confirmed.has_value());
        QVERIFY(confirmed->connectedAt.isValid());
        QCOMPARE(confirmed->direction, tincan::CallDirection::Outgoing);

        QVERIFY(engine_->hangup(callId));
        QTRY_VERIFY_WITH_TIMEOUT(
            events_->hasCallState(callId, tincan::CallState::Disconnected), 10000);
        QVERIFY(engine_->calls().isEmpty());
    }

    void outgoingCallBusy486() {
        tincan::test::PjsuaPeer peer;
        QVERIFY2(peer.start(486, 30), qPrintable(peer.output()));

        QString error;
        const int callId = engine_->makeCall(QStringLiteral("local"), peer.sipUri(), &error);
        QVERIFY2(callId >= 0, qPrintable(error));

        QTRY_VERIFY_WITH_TIMEOUT(
            events_->hasCallState(callId, tincan::CallState::Disconnected), 10000);
        const auto disc = events_->findCallState(callId, tincan::CallState::Disconnected);
        QVERIFY(disc.has_value());
        QCOMPARE(disc->lastStatusCode, 486);
        QVERIFY(!disc->connectedAt.isValid());
        QVERIFY(engine_->calls().isEmpty());
    }

    void incomingCallAnsweredAndHungUp() {
        QSignalSpy incomingSpy(events_.get(), &tincan::test::RecordingEvents::incomingCallReceived);
        const QString target =
            QStringLiteral("sip:tincan@127.0.0.1:%1").arg(engine_->localUdpPort());
        tincan::test::PjsuaPeer peer;
        QVERIFY2(peer.start(200, 30, target), qPrintable(peer.output()));

        QTRY_VERIFY_WITH_TIMEOUT(incomingSpy.count() >= 1, 10000);
        const tincan::CallInfo inc = events_->incomingCalls().front();
        QCOMPARE(inc.state, tincan::CallState::Incoming);
        QCOMPARE(inc.direction, tincan::CallDirection::Incoming);

        QVERIFY(engine_->answer(inc.callId));
        QTRY_VERIFY_WITH_TIMEOUT(
            events_->hasCallState(inc.callId, tincan::CallState::Confirmed), 10000);

        QVERIFY(engine_->hangup(inc.callId));
        QTRY_VERIFY_WITH_TIMEOUT(
            events_->hasCallState(inc.callId, tincan::CallState::Disconnected), 10000);
        QVERIFY(engine_->calls().isEmpty());
    }

    void incomingCallRejectedWith603() {
        const QString target =
            QStringLiteral("sip:tincan@127.0.0.1:%1").arg(engine_->localUdpPort());
        tincan::test::PjsuaPeer peer;
        QVERIFY2(peer.start(200, 30, target), qPrintable(peer.output()));

        QTRY_VERIFY_WITH_TIMEOUT(!events_->incomingCalls().isEmpty(), 10000);
        const tincan::CallInfo inc = events_->incomingCalls().front();
        QVERIFY(engine_->reject(inc.callId, 603));

        QTRY_VERIFY_WITH_TIMEOUT(
            events_->hasCallState(inc.callId, tincan::CallState::Disconnected), 10000);
        QTRY_VERIFY_WITH_TIMEOUT(peer.output().contains(QStringLiteral("603")), 10000);
        QVERIFY(engine_->calls().isEmpty());
    }

    void outgoingCallNotFound404() {
        tincan::test::PjsuaPeer peer;
        QVERIFY2(peer.start(404, 30), qPrintable(peer.output()));

        QString error;
        const int callId = engine_->makeCall(QStringLiteral("local"), peer.sipUri(), &error);
        QVERIFY2(callId >= 0, qPrintable(error));

        QTRY_VERIFY_WITH_TIMEOUT(
            events_->hasCallState(callId, tincan::CallState::Disconnected), 10000);
        const auto disc = events_->findCallState(callId, tincan::CallState::Disconnected);
        QVERIFY(disc.has_value());
        QVERIFY2(disc->lastStatusCode >= 400 && disc->lastStatusCode < 600,
                 qPrintable(QStringLiteral("Expected 4xx/5xx, got %1").arg(disc->lastStatusCode)));
        QCOMPARE(disc->lastStatusCode, 404);
        QVERIFY(engine_->calls().isEmpty());
    }

private:
    std::unique_ptr<tincan::test::RecordingEvents> events_;
    std::unique_ptr<tincan::sip::PjsipEngine> engine_;
};

QTEST_GUILESS_MAIN(TestE2eCalls)
#include "tst_e2e_calls.moc"
