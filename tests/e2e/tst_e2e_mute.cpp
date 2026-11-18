// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "core/account_config.h"
#include "core/engine_config.h"
#include "pjsua_peer.h"
#include "recording_events.h"
#include "sip/pjsip_engine.h"

#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

// Mute must survive media renegotiation: a re-INVITE (here hold/resume)
// must never reconnect the microphone behind the user's back.
class TestE2eMute : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() {
        events_ = std::make_unique<tincan::test::RecordingEvents>();
        engine_ = std::make_unique<tincan::sip::PjsipEngine>(*events_);
        tincan::EngineConfig cfg;
        cfg.udpPort = 0;
        cfg.enableTcp = false;
        cfg.nullAudio = true;
        QString error;
        QVERIFY2(engine_->start(cfg, &error), qPrintable(error));
        tincan::AccountConfig localAcc;
        localAcc.id = QStringLiteral("local");
        localAcc.user = QStringLiteral("tincan");
        localAcc.domain = QStringLiteral("127.0.0.1");
        localAcc.registerAccount = false;
        QVERIFY2(engine_->addAccount(localAcc, &error), qPrintable(error));
    }

    void cleanupTestCase() {
        engine_->shutdown();
        engine_.reset();
        events_.reset();
    }

    void muteSurvivesReinvite() {
        tincan::test::PjsuaPeer peer;
        QVERIFY2(peer.start(200, 30), qPrintable(peer.output()));
        QString error;
        const int callId = engine_->makeCall(QStringLiteral("local"), peer.sipUri(), &error);
        QVERIFY2(callId >= 0, qPrintable(error));
        QTRY_VERIFY_WITH_TIMEOUT(micConnected(callId), 10000);

        // pjmedia's conference bridge applies connect/disconnect operations
        // asynchronously, so routing changes are polled for.
        QVERIFY(engine_->setMute(callId, true));
        QTRY_VERIFY_WITH_TIMEOUT(!micConnected(callId), 2000);

        for (int round = 0; round < 3; ++round) {
            Q_UNUSED(round);
            QVERIFY(engine_->setHold(callId, true));
            QTRY_VERIFY_WITH_TIMEOUT(callInfo(callId).localHold, 10000);
            events_->clear();
            QVERIFY(engine_->setHold(callId, false));
            // The resumed-media event is delivered on the main thread after
            // capture routing has run for it.
            QTRY_VERIFY_WITH_TIMEOUT(resumedEventSeen(callId), 10000);
            // Let any queued bridge operation land, then require the mic to be
            // off: polling here could pass before a buggy reconnect applied.
            QTest::qWait(500);
            QVERIFY2(!micConnected(callId), "microphone reconnected while muted");
            QVERIFY(callInfo(callId).muted);
        }

        QVERIFY(engine_->setMute(callId, false));
        QTRY_VERIFY_WITH_TIMEOUT(micConnected(callId), 2000);
        QVERIFY(engine_->hangup(callId));
        QTRY_VERIFY_WITH_TIMEOUT(engine_->calls().isEmpty(), 5000);
    }

    // While muted, the microphone must not feed the call recording either;
    // muted spans are recorded as silence on our side.
    void recordingSilentWhileMuted() {
        tincan::test::PjsuaPeer peer;
        QVERIFY2(peer.start(200, 30), qPrintable(peer.output()));
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QString error;
        const int callId = engine_->makeCall(QStringLiteral("local"), peer.sipUri(), &error);
        QVERIFY2(callId >= 0, qPrintable(error));
        QTRY_VERIFY_WITH_TIMEOUT(micConnected(callId), 10000);

        QVERIFY(engine_->startRecording(callId, dir.filePath(QStringLiteral("call.wav"))));
        QTRY_VERIFY_WITH_TIMEOUT(callInfo(callId).micRecorded, 2000);
        QVERIFY(engine_->setMute(callId, true));
        QTRY_VERIFY_WITH_TIMEOUT(!callInfo(callId).micRecorded, 2000);

        // A re-INVITE re-syncs the recorder; it must stay off the microphone.
        events_->clear();
        QVERIFY(engine_->setHold(callId, true));
        QTRY_VERIFY_WITH_TIMEOUT(callInfo(callId).localHold, 10000);
        QVERIFY(engine_->setHold(callId, false));
        QTRY_VERIFY_WITH_TIMEOUT(resumedEventSeen(callId), 10000);
        QTest::qWait(500);
        QVERIFY2(!callInfo(callId).micRecorded, "recording picked up the muted microphone");

        QVERIFY(engine_->setMute(callId, false));
        QTRY_VERIFY_WITH_TIMEOUT(callInfo(callId).micRecorded, 2000);
        engine_->stopRecording(callId);
        QVERIFY(engine_->hangup(callId));
        QTRY_VERIFY_WITH_TIMEOUT(engine_->calls().isEmpty(), 5000);
        QVERIFY(QFileInfo(dir.filePath(QStringLiteral("call.wav"))).size() > 0);
    }

private:
    tincan::CallInfo callInfo(int callId) const {
        for (const tincan::CallInfo &call : engine_->calls()) {
            if (call.callId == callId) {
                return call;
            }
        }
        return {};
    }

    bool micConnected(int callId) const { return callInfo(callId).micConnected; }

    bool resumedEventSeen(int callId) const {
        for (const tincan::CallInfo &change : events_->callChanges()) {
            if (change.callId == callId && !change.localHold &&
                change.state == tincan::CallState::Confirmed) {
                return true;
            }
        }
        return false;
    }

    std::unique_ptr<tincan::test::RecordingEvents> events_;
    std::unique_ptr<tincan::sip::PjsipEngine> engine_;
};

QTEST_GUILESS_MAIN(TestE2eMute)
#include "tst_e2e_mute.moc"
