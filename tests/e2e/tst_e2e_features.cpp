// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "core/account_config.h"
#include "core/dtmf_method.h"
#include "core/engine_config.h"
#include "pjsua_peer.h"
#include "recording_events.h"
#include "sip/pjsip_engine.h"

#include <QElapsedTimer>
#include <QFileInfo>
#include <QHostAddress>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTest>
#include <QUdpSocket>

#include <functional>
#include <memory>

namespace {

bool hasCallMatching(const QList<tincan::CallInfo> &changes,
                     const std::function<bool(const tincan::CallInfo &)> &pred) {
    for (auto it = changes.crbegin(); it != changes.crend(); ++it) {
        if (pred(*it)) {
            return true;
        }
    }
    return false;
}

QByteArray buildHeaderTestInvite(int dstPort, quint16 srcPort, const QString &callId) {
    return QStringLiteral("INVITE sip:tincan@127.0.0.1:%1 SIP/2.0\r\n"
                          "Via: SIP/2.0/UDP 127.0.0.1:%2;branch=z9hG4bK-hdr-%2;rport\r\n"
                          "Max-Forwards: 70\r\n"
                          "From: <sip:tester@127.0.0.1:%2>;tag=hdrtag1\r\n"
                          "To: <sip:tincan@127.0.0.1:%1>\r\n"
                          "Call-ID: %3\r\n"
                          "CSeq: 1 INVITE\r\n"
                          "Contact: <sip:tester@127.0.0.1:%2>\r\n"
                          "Call-Info: <sip:x>;answer-after=0\r\n"
                          "Alert-Info: <http://127.0.0.1>;info=intercom\r\n"
                          "Answer-Mode: Auto\r\n"
                          "Priority: emergency\r\n"
                          "X-Custom-Ignored: secret\r\n"
                          "Content-Length: 0\r\n\r\n")
        .arg(dstPort)
        .arg(srcPort)
        .arg(callId)
        .toUtf8();
}

void sendAckForRejectedInvite(QUdpSocket &sender, int dstPort, quint16 srcPort,
                              const QString &callId) {
    QString toHeader = QStringLiteral("<sip:tincan@127.0.0.1:%1>").arg(dstPort);
    while (sender.hasPendingDatagrams()) {
        QByteArray datagram(static_cast<qsizetype>(sender.pendingDatagramSize()), Qt::Uninitialized);
        sender.readDatagram(datagram.data(), datagram.size());
        const QString msg = QString::fromUtf8(datagram);
        if (msg.startsWith(QStringLiteral("SIP/2.0 603"))) {
            static const QRegularExpression toRe(QStringLiteral("(?m)^To:\\s*(.+)\\r?$"),
                                                 QRegularExpression::CaseInsensitiveOption);
            const auto m = toRe.match(msg);
            if (m.hasMatch()) {
                toHeader = m.captured(1).trimmed();
            }
        }
    }
    const QString ack = QStringLiteral("ACK sip:tincan@127.0.0.1:%1 SIP/2.0\r\n"
                                       "Via: SIP/2.0/UDP 127.0.0.1:%2;branch=z9hG4bK-hdr-%2;rport\r\n"
                                       "Max-Forwards: 70\r\n"
                                       "From: <sip:tester@127.0.0.1:%2>;tag=hdrtag1\r\n"
                                       "To: %3\r\n"
                                       "Call-ID: %4\r\n"
                                       "CSeq: 1 ACK\r\n"
                                       "Content-Length: 0\r\n\r\n")
                            .arg(dstPort)
                            .arg(srcPort)
                            .arg(toHeader, callId);
    (void)sender.writeDatagram(ack.toUtf8(), QHostAddress::LocalHost,
                               static_cast<quint16>(dstPort));
}

} // namespace

class TestE2eFeatures : public QObject {
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
        cfg.userAgent = QStringLiteral("Tincan-E2E-P3/0.1");

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

    void dtmfRfc4733AndSipInfoReceivedByPeer() {
        tincan::test::PjsuaPeer peer;
        QVERIFY2(peer.start(200, 30), qPrintable(peer.output()));

        QString error;
        const int callId = engine_->makeCall(QStringLiteral("local"), peer.sipUri(), &error);
        QVERIFY2(callId >= 0, qPrintable(error));
        QTRY_VERIFY_WITH_TIMEOUT(
            events_->hasCallState(callId, tincan::CallState::Confirmed), 10000);

        QVERIFY(engine_->sendDtmf(callId, QStringLiteral("5"), tincan::DtmfMethod::Rfc4733));
        QTRY_VERIFY_WITH_TIMEOUT(
            peer.output().contains(QStringLiteral(": 5, using RFC2833 method")), 10000);

        QVERIFY(engine_->sendDtmf(callId, QStringLiteral("8"), tincan::DtmfMethod::SipInfo));
        QTRY_VERIFY_WITH_TIMEOUT(
            peer.output().contains(QStringLiteral("Incoming DTMF on call")) &&
                peer.output().contains(QStringLiteral("using SIP INFO method")) &&
                peer.output().contains(QStringLiteral(": 8")),
            10000);

        QVERIFY(engine_->sendDtmf(callId, QStringLiteral("1"), tincan::DtmfMethod::InBand));

        QVERIFY(engine_->hangup(callId));
        QTRY_VERIFY_WITH_TIMEOUT(engine_->calls().isEmpty(), 5000);
    }

    void holdThenResumeUpdatesCallInfoAndPeerMediaState() {
        tincan::test::PjsuaPeer peer;
        QVERIFY2(peer.start(200, 30), qPrintable(peer.output()));

        QString error;
        const int callId = engine_->makeCall(QStringLiteral("local"), peer.sipUri(), &error);
        QVERIFY2(callId >= 0, qPrintable(error));
        QTRY_VERIFY_WITH_TIMEOUT(
            events_->hasCallState(callId, tincan::CallState::Confirmed), 10000);

        QVERIFY(engine_->setHold(callId, true));
        QTRY_VERIFY_WITH_TIMEOUT(
            hasCallMatching(events_->callChanges(),
                            [callId](const tincan::CallInfo &c) {
                                return c.callId == callId && c.localHold;
                            }),
            10000);
        QTRY_VERIFY_WITH_TIMEOUT(
            peer.output().contains(QStringLiteral("status is Remote hold")), 10000);

        events_->clear();
        QVERIFY(engine_->setHold(callId, false));
        QTRY_VERIFY_WITH_TIMEOUT(
            hasCallMatching(events_->callChanges(),
                            [callId](const tincan::CallInfo &c) {
                                return c.callId == callId &&
                                       c.state == tincan::CallState::Confirmed && !c.localHold;
                            }),
            10000);
        QTRY_VERIFY_WITH_TIMEOUT(
            peer.output().lastIndexOf(QStringLiteral("status is Active")) >
                peer.output().lastIndexOf(QStringLiteral("status is Remote hold")),
            10000);

        QVERIFY(engine_->hangup(callId));
        QTRY_VERIFY_WITH_TIMEOUT(engine_->calls().isEmpty(), 5000);
    }

    void blindTransferAndIncomingReferConnectToTargetPeer() {
        tincan::test::PjsuaPeer peerA;
        tincan::test::PjsuaPeer peerB;
        QVERIFY2(peerA.start(200, 30), qPrintable(peerA.output()));
        QVERIFY2(peerB.start(200, 30), qPrintable(peerB.output()));

        QString error;
        const int callId = engine_->makeCall(QStringLiteral("local"), peerA.sipUri(), &error);
        QVERIFY2(callId >= 0, qPrintable(error));
        QTRY_VERIFY_WITH_TIMEOUT(
            events_->hasCallState(callId, tincan::CallState::Confirmed), 10000);

        QVERIFY(engine_->transferBlind(callId, peerB.sipUri()));
        QTRY_VERIFY_WITH_TIMEOUT(
            peerB.output().contains(QStringLiteral("Answering call 0: code=200")), 10000);
        QTRY_VERIFY_WITH_TIMEOUT(
            events_->hasCallState(callId, tincan::CallState::Disconnected), 10000);
        QVERIFY(hasCallMatching(events_->callChanges(), [callId](const tincan::CallInfo &c) {
            return c.callId == callId && c.transferStatus >= 200 && c.transferStatus < 300;
        }));
        QVERIFY(engine_->calls().isEmpty());

        // Incoming REFER: peerA transfers Tincan to peerB; child SipCall is owned by CallRegistry.
        const int refCallId = engine_->makeCall(QStringLiteral("local"), peerA.sipUri(), &error);
        QVERIFY2(refCallId >= 0, qPrintable(error));
        QTRY_VERIFY_WITH_TIMEOUT(
            events_->hasCallState(refCallId, tincan::CallState::Confirmed), 10000);

        peerA.sendInput("x\n" + peerB.sipUri().toUtf8() + "\n");
        QTRY_VERIFY_WITH_TIMEOUT(
            hasCallMatching(events_->callChanges(),
                            [refCallId](const tincan::CallInfo &c) {
                                return c.callId != refCallId &&
                                       c.state == tincan::CallState::Confirmed;
                            }),
            10000);

        for (const tincan::CallInfo &c : engine_->calls()) {
            (void)engine_->hangup(c.callId);
        }
        QTRY_VERIFY_WITH_TIMEOUT(engine_->calls().isEmpty(), 5000);
    }

    void conferenceAndAttendedTransferAcrossTwoPeers() {
        tincan::test::PjsuaPeer peerA;
        tincan::test::PjsuaPeer peerB;
        QVERIFY2(peerA.start(200, 30), qPrintable(peerA.output()));
        QVERIFY2(peerB.start(200, 30), qPrintable(peerB.output()));

        QString error;
        const int callA = engine_->makeCall(QStringLiteral("local"), peerA.sipUri(), &error);
        const int callB = engine_->makeCall(QStringLiteral("local"), peerB.sipUri(), &error);
        QVERIFY(callA >= 0 && callB >= 0);
        QTRY_VERIFY_WITH_TIMEOUT(
            events_->hasCallState(callA, tincan::CallState::Confirmed) &&
                events_->hasCallState(callB, tincan::CallState::Confirmed),
            10000);

        QVERIFY(engine_->conference({callA, callB}));
        QVERIFY(engine_->unconference(callA));

        QVERIFY(engine_->transferAttended(callA, callB));
        QTRY_VERIFY_WITH_TIMEOUT(engine_->calls().isEmpty(), 10000);
    }

    void recordingConnectedCallWritesNonEmptyWav() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString wavPath = tempDir.filePath(QStringLiteral("call_rec.wav"));

        tincan::test::PjsuaPeer peer;
        QVERIFY2(peer.start(200, 30), qPrintable(peer.output()));

        QString error;
        const int callId = engine_->makeCall(QStringLiteral("local"), peer.sipUri(), &error);
        QVERIFY2(callId >= 0, qPrintable(error));
        QTRY_VERIFY_WITH_TIMEOUT(
            events_->hasCallState(callId, tincan::CallState::Confirmed), 10000);

        QVERIFY(engine_->startRecording(callId, wavPath));
        QVERIFY(engine_->setMute(callId, true));
        QVERIFY(engine_->setMute(callId, false));
        QVERIFY(engine_->setCallVolume(callId, 1.2f, 1.0f));

        QElapsedTimer recTimer;
        recTimer.start();
        QTRY_VERIFY_WITH_TIMEOUT(recTimer.elapsed() >= 2000 && QFileInfo(wavPath).exists(), 5000);
        engine_->stopRecording(callId);

        const QFileInfo wavInfo(wavPath);
        QVERIFY(wavInfo.exists());
        QVERIFY2(wavInfo.size() > 44,
                 qPrintable(QStringLiteral("WAV size was %1 bytes").arg(wavInfo.size())));

        QVERIFY(engine_->hangup(callId));
        QTRY_VERIFY_WITH_TIMEOUT(engine_->calls().isEmpty(), 5000);
    }

    void redirect302ForwardsPeerToTargetPeer() {
        tincan::test::PjsuaPeer peerB;
        QVERIFY2(peerB.start(200, 30), qPrintable(peerB.output()));

        const QString tincanUri =
            QStringLiteral("sip:tincan@127.0.0.1:%1").arg(engine_->localUdpPort());
        tincan::test::PjsuaPeer peerA;
        QVERIFY2(peerA.start(200, 30, tincanUri), qPrintable(peerA.output()));

        QTRY_VERIFY_WITH_TIMEOUT(!events_->incomingCalls().isEmpty(), 10000);
        const int incCallId = events_->incomingCalls().front().callId;
        QVERIFY(engine_->redirect(incCallId, peerB.sipUri()));

        QTRY_VERIFY_WITH_TIMEOUT(
            peerB.output().contains(QStringLiteral("Answering call 0: code=200")), 10000);
        QTRY_VERIFY_WITH_TIMEOUT(
            events_->hasCallState(incCallId, tincan::CallState::Disconnected), 10000);
    }

    void incomingInviteExtractsAllowedHeadersOnEngine() {
        QUdpSocket sender;
        QVERIFY(sender.bind(QHostAddress::LocalHost, 0));
        const quint16 srcPort = sender.localPort();
        const int dstPort = engine_->localUdpPort();
        const QString callIdHdr = QStringLiteral("hdr-test-%1@127.0.0.1").arg(srcPort);
        const QByteArray rawInvite = buildHeaderTestInvite(dstPort, srcPort, callIdHdr);

        QVERIFY(sender.writeDatagram(rawInvite, QHostAddress::LocalHost,
                                     static_cast<quint16>(dstPort)) > 0);
        QTRY_VERIFY_WITH_TIMEOUT(!events_->incomingCalls().isEmpty(), 5000);
        const tincan::CallInfo inc = events_->incomingCalls().last();
        QCOMPARE(inc.incomingHeaders.size(), 4);
        QCOMPARE(inc.incomingHeaders.value(QStringLiteral("Call-Info")),
                 QStringLiteral("<sip:x>;answer-after=0"));
        QCOMPARE(inc.incomingHeaders.value(QStringLiteral("Alert-Info")),
                 QStringLiteral("<http://127.0.0.1>;info=intercom"));
        QCOMPARE(inc.incomingHeaders.value(QStringLiteral("Answer-Mode")), QStringLiteral("Auto"));
        QCOMPARE(inc.incomingHeaders.value(QStringLiteral("Priority")),
                 QStringLiteral("emergency"));

        QVERIFY(engine_->reject(inc.callId, 603));
        QTRY_VERIFY_WITH_TIMEOUT(sender.hasPendingDatagrams(), 2000);
        sendAckForRejectedInvite(sender, dstPort, srcPort, callIdHdr);
        QTRY_VERIFY_WITH_TIMEOUT(engine_->calls().isEmpty(), 5000);
    }

private:
    std::unique_ptr<tincan::test::RecordingEvents> events_;
    std::unique_ptr<tincan::sip::PjsipEngine> engine_;
};

QTEST_GUILESS_MAIN(TestE2eFeatures)
#include "tst_e2e_features.moc"
