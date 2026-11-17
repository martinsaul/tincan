// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "core/account_config.h"
#include "core/engine_config.h"
#include "pjsua_peer.h"
#include "recording_events.h"
#include "sip/pjsip_engine.h"

#include <QHostAddress>
#include <QTest>
#include <QUdpSocket>

#include <memory>

namespace {

bool hasPresenceState(const QList<tincan::PresenceInfo> &events, tincan::PresenceState target) {
    for (auto it = events.crbegin(); it != events.crend(); ++it) {
        if (it->state == target) {
            return true;
        }
    }
    return false;
}

QByteArray buildRawSipMessage(int dstPort, quint16 srcPort, const QString &body) {
    const QByteArray bodyUtf8 = body.toUtf8();
    return QStringLiteral("MESSAGE sip:tincan@127.0.0.1:%1 SIP/2.0\r\n"
                          "Via: SIP/2.0/UDP 127.0.0.1:%2;branch=z9hG4bK-msg-%2;rport\r\n"
                          "Max-Forwards: 70\r\n"
                          "From: \"Alice Peer\" <sip:alice@127.0.0.1:%2>;tag=msgtag1\r\n"
                          "To: <sip:tincan@127.0.0.1:%1>\r\n"
                          "Call-ID: msg-test-%2@127.0.0.1\r\n"
                          "CSeq: 1 MESSAGE\r\n"
                          "Content-Type: text/plain\r\n"
                          "Content-Length: %3\r\n\r\n")
               .arg(dstPort)
               .arg(srcPort)
               .arg(bodyUtf8.size())
               .toUtf8() +
           bodyUtf8;
}

} // namespace

class TestE2eP4 : public QObject {
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
        cfg.userAgent = QStringLiteral("Tincan-E2E-P4/0.1");

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

    void cleanup() { events_->clear(); }

    void cleanupTestCase() {
        if (engine_) {
            engine_->shutdown();
            engine_.reset();
        }
        events_.reset();
    }

    void tincanSendsMessageToPeerAndReceives200Status() {
        tincan::test::PjsuaPeer peer;
        QVERIFY2(peer.start(200, 30), qPrintable(peer.output()));

        const QString body = QStringLiteral("Hello from Tincan P4!");
        const quint64 token = 4201;
        QVERIFY(engine_->sendMessage(QStringLiteral("local"), peer.sipUri(), body, token));

        QTRY_VERIFY_WITH_TIMEOUT(
            peer.output().contains(QStringLiteral("MESSAGE from ")) &&
                peer.output().contains(
                    QStringLiteral(": Hello from Tincan P4! (text/plain)")),
            10000);

        QTRY_VERIFY_WITH_TIMEOUT(!events_->messageStatuses().isEmpty(), 10000);
        const auto st = events_->messageStatuses().last();
        QCOMPARE(st.token, token);
        QCOMPARE(st.statusCode, 200);
    }

    void peerSendsMessageToTincanViaInteractiveMenuAndRawDisplayName() {
        tincan::test::PjsuaPeer peer;
        QVERIFY2(peer.start(200, 30), qPrintable(peer.output()));

        const QString tincanUri =
            QStringLiteral("sip:tincan@127.0.0.1:%1").arg(engine_->localUdpPort());
        const QString body = QStringLiteral("Hello back from pjsua!");
        peer.sendInput("i\n" + tincanUri.toUtf8() + "\n" + body.toUtf8() + "\n");

        QTRY_VERIFY_WITH_TIMEOUT(!events_->incomingMessages().isEmpty(), 10000);
        const tincan::MessageInfo msg = events_->incomingMessages().last();
        QCOMPARE(msg.accountId, QStringLiteral("local"));
        QCOMPARE(msg.body, body);
        QCOMPARE(msg.contentType, QStringLiteral("text/plain"));
        QVERIFY(msg.fromUri.contains(QStringLiteral("127.0.0.1")));
        QVERIFY(msg.receivedAt.isValid());

        // Verify display-name extraction and 200 OK response with a named From header.
        events_->clear();
        QUdpSocket sender;
        QVERIFY(sender.bind(QHostAddress::LocalHost, 0));
        const int dstPort = engine_->localUdpPort();
        const QByteArray raw =
            buildRawSipMessage(dstPort, sender.localPort(), QStringLiteral("Named sender ping"));
        QVERIFY(sender.writeDatagram(raw, QHostAddress::LocalHost, static_cast<quint16>(dstPort)) >
                0);

        QTRY_VERIFY_WITH_TIMEOUT(!events_->incomingMessages().isEmpty(), 5000);
        const tincan::MessageInfo namedMsg = events_->incomingMessages().last();
        QCOMPARE(namedMsg.fromName, QStringLiteral("Alice Peer"));
        QCOMPARE(namedMsg.body, QStringLiteral("Named sender ping"));
        QTRY_VERIFY_WITH_TIMEOUT(sender.hasPendingDatagrams(), 2000);
    }

    void presenceSubscriptionYieldsOnlineAndRpidUpdates() {
        tincan::test::PjsuaPeer peer;
        QVERIFY2(peer.start(200, 30), qPrintable(peer.output()));

        QVERIFY(engine_->setOwnPresence(QStringLiteral("local"), tincan::PresenceState::Online));
        QVERIFY(engine_->subscribePresence(QStringLiteral("local"), peer.sipUri()));

        // pjsua enables online_status=PJ_TRUE on its local UDP account at startup,
        // accepts SUBSCRIBE with 200 OK, and sends PIDF NOTIFY (<basic>open</basic>).
        QTRY_VERIFY_WITH_TIMEOUT(
            hasPresenceState(events_->presenceChanges(), tincan::PresenceState::Online), 10000);
        QCOMPARE(events_->presenceChanges().last().uri, peer.sipUri());

        // Switch pjsua extended status via 'T' -> '3' ("On the phone" RPID busy + note).
        events_->clear();
        peer.sendInput("T\n3\n");
        QTRY_VERIFY_WITH_TIMEOUT(
            hasPresenceState(events_->presenceChanges(), tincan::PresenceState::OnThePhone), 10000);

        engine_->unsubscribePresence(peer.sipUri());
    }

private:
    std::unique_ptr<tincan::test::RecordingEvents> events_;
    std::unique_ptr<tincan::sip::PjsipEngine> engine_;
};

QTEST_GUILESS_MAIN(TestE2eP4)
#include "tst_e2e_p4.moc"
