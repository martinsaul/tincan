// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "core/account_config.h"
#include "core/engine_config.h"
#include "pjsua_peer.h"
#include "recording_events.h"
#include "sip/pjsip_engine.h"

#include <QElapsedTimer>
#include <QHash>
#include <QThread>
#include <QTest>

#include <memory>

// pjsua hands out call ids round-robin from a fixed pool and pjsua2 keeps a
// Call's id after it has disconnected. With a one-call pool every call reuses
// the same id, often while the previous call's removal is still queued on the
// main thread. Every call must still connect and disconnect cleanly, and
// nothing may be left behind.
class TestE2eReuse : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() {
        events_ = std::make_unique<tincan::test::RecordingEvents>();
        engine_ = std::make_unique<tincan::sip::PjsipEngine>(*events_);
        tincan::EngineConfig cfg;
        cfg.udpPort = 0;
        cfg.enableTcp = false;
        cfg.nullAudio = true;
        cfg.maxCalls = 1;
        QString error;
        QVERIFY2(engine_->start(cfg, &error), qPrintable(error));
        tincan::AccountConfig localAcc;
        localAcc.id = QStringLiteral("local");
        localAcc.user = QStringLiteral("tincan");
        localAcc.domain = QStringLiteral("127.0.0.1");
        localAcc.registerAccount = false;
        QVERIFY2(engine_->addAccount(localAcc, &error), qPrintable(error));
    }

    void init() { events_->clear(); }

    void cleanupTestCase() {
        engine_->shutdown();
        engine_.reset();
        events_.reset();
    }

    void rapidCallsBeyondPoolSize() {
        tincan::test::PjsuaPeer peer;
        QVERIFY2(peer.start(200, 120), qPrintable(peer.output()));
        constexpr int kCalls = 12;
        QHash<int, int> callsPerId;
        for (int i = 0; i < kCalls; ++i) {
            const int callId = dialAsSoonAsFree(peer.sipUri());
            QVERIFY2(callId >= 0, qPrintable(QStringLiteral("call %1 not placed").arg(i)));
            const int expected = ++callsPerId[callId];
            QTRY_VERIFY2_WITH_TIMEOUT(countState(callId, tincan::CallState::Confirmed) >= expected,
                                      qPrintable(QStringLiteral("call %1 never connected").arg(i)),
                                      10000);
            QVERIFY(engine_->hangup(callId));
            // No wait for the disconnect: the next call is dialled at once.
        }
        QVERIFY2(callsPerId.size() < kCalls, "no call id was reused");
        expectAllDisconnected(callsPerId);
    }

    // Deterministic version of the race: the old call's DISCONNECTED event is
    // still queued (the main thread is kept busy) when its id is handed to a
    // new call. Releasing the old object must not touch the new call.
    void idReusedWhileOldCallRemovalIsQueued() {
        tincan::test::PjsuaPeer peer;
        QVERIFY2(peer.start(200, 120), qPrintable(peer.output()));
        QHash<int, int> callsPerId;
        for (int round = 0; round < 3; ++round) {
            const int oldId = dialAsSoonAsFree(peer.sipUri());
            QVERIFY(oldId >= 0);
            const int expected = ++callsPerId[oldId];
            QTRY_VERIFY_WITH_TIMEOUT(countState(oldId, tincan::CallState::Confirmed) >= expected,
                                     10000);
            QVERIFY(engine_->hangup(oldId));
            const int newId = dialWithoutEventLoop(peer.sipUri());
            QCOMPARE(newId, oldId);
            ++callsPerId[newId];
            QTRY_VERIFY2_WITH_TIMEOUT(
                countState(newId, tincan::CallState::Confirmed) >= expected + 1,
                "the call that reused the id was disturbed", 10000);
            QTest::qWait(300);
            QVERIFY2(!engine_->calls().isEmpty(), "the call that reused the id was ended");
            QVERIFY(engine_->hangup(newId));
            QTRY_VERIFY_WITH_TIMEOUT(engine_->calls().isEmpty(), 5000);
        }
        expectAllDisconnected(callsPerId);
    }

private:
    // Dials, retrying while the one-call pool is still busy; the event loop
    // keeps running, so queued removals may or may not run first.
    int dialAsSoonAsFree(const QString &uri) {
        QElapsedTimer timer;
        timer.start();
        while (timer.elapsed() < 5000) {
            const int callId = engine_->makeCall(QStringLiteral("local"), uri, nullptr);
            if (callId >= 0) {
                return callId;
            }
            QTest::qWait(5);
        }
        return -1;
    }

    // Dials as soon as PJSIP has freed the pool slot, without letting the
    // main thread process the queued DISCONNECTED event of the previous call.
    int dialWithoutEventLoop(const QString &uri) {
        QElapsedTimer timer;
        timer.start();
        while (timer.elapsed() < 5000) {
            const int callId = engine_->makeCall(QStringLiteral("local"), uri, nullptr);
            if (callId >= 0) {
                return callId;
            }
            QThread::msleep(2);
        }
        return -1;
    }

    void expectAllDisconnected(const QHash<int, int> &callsPerId) {
        QTRY_VERIFY_WITH_TIMEOUT(engine_->calls().isEmpty(), 10000);
        for (auto it = callsPerId.cbegin(); it != callsPerId.cend(); ++it) {
            QTRY_COMPARE_WITH_TIMEOUT(countState(it.key(), tincan::CallState::Disconnected),
                                      it.value(), 5000);
        }
        // Nothing lingers after the queued removals have all run.
        QTest::qWait(200);
        QVERIFY(engine_->calls().isEmpty());
    }

    int countState(int callId, tincan::CallState state) const {
        int count = 0;
        for (const tincan::CallInfo &change : events_->callChanges()) {
            if (change.callId == callId && change.state == state) {
                ++count;
            }
        }
        return count;
    }

    std::unique_ptr<tincan::test::RecordingEvents> events_;
    std::unique_ptr<tincan::sip::PjsipEngine> engine_;
};

QTEST_GUILESS_MAIN(TestE2eReuse)
#include "tst_e2e_reuse.moc"
