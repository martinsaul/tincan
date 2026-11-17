// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "core/sip_headers.h"

#include <QTest>

class TestSipHeaders : public QObject {
    Q_OBJECT

private slots:
    void extractsOnlyAllowedHeadersCaseInsensitively() {
        const QString invite = QStringLiteral(
            "INVITE sip:100@127.0.0.1 SIP/2.0\r\n"
            "Via: SIP/2.0/UDP 127.0.0.1:5060;branch=z9hG4bK123\r\n"
            "From: \"Alice\" <sip:alice@example.com>;tag=abc\r\n"
            "To: <sip:100@127.0.0.1>\r\n"
            "call-info: <sip:pbx.example.com>;answer-after=2\r\n"
            "ALERT-INFO : <http://127.0.0.1/ring>;info=alert-autoanswer\r\n"
            "Answer-Mode: Auto;require\r\n"
            "priority: urgent\r\n"
            "User-Agent: TestPeer/1.0\r\n"
            "Content-Length: 18\r\n"
            "\r\n"
            "Call-Info: fake-body-line\r\n");

        const auto parsed = tincan::SipHeaders::parseIncomingHeaders(invite);
        QCOMPARE(parsed.size(), 4);
        QCOMPARE(parsed.value(QStringLiteral("Call-Info")),
                 QStringLiteral("<sip:pbx.example.com>;answer-after=2"));
        QCOMPARE(parsed.value(QStringLiteral("Alert-Info")),
                 QStringLiteral("<http://127.0.0.1/ring>;info=alert-autoanswer"));
        QCOMPARE(parsed.value(QStringLiteral("Answer-Mode")), QStringLiteral("Auto;require"));
        QCOMPARE(parsed.value(QStringLiteral("Priority")), QStringLiteral("urgent"));
    }

    void handlesCompactMessageWithoutConfusingCallIdAndSupportsFolding() {
        const QString compactInvite = QStringLiteral(
            "INVITE sip:tincan@127.0.0.1 SIP/2.0\r\n"
            "v: SIP/2.0/UDP 127.0.0.1:5090;branch=z9hG4bK99\r\n"
            "f: <sip:bob@127.0.0.1>;tag=11\r\n"
            "t: <sip:tincan@127.0.0.1>\r\n"
            "i: 99887766@127.0.0.1\r\n"
            "m: <sip:bob@127.0.0.1:5090>\r\n"
            "Call-Info: <sip:pbx>;purpose=icon,\r\n"
            " \t<sip:pbx>;answer-after=0\r\n"
            "Call-Info: <sip:extra>;purpose=info\r\n"
            "c: application/sdp\r\n"
            "l: 0\r\n"
            "\r\n");

        const auto parsed = tincan::SipHeaders::parseIncomingHeaders(compactInvite);
        QCOMPARE(parsed.size(), 1);
        QCOMPARE(parsed.value(QStringLiteral("Call-Info")),
                 QStringLiteral("<sip:pbx>;purpose=icon, <sip:pbx>;answer-after=0, "
                                "<sip:extra>;purpose=info"));
    }
};

QTEST_APPLESS_MAIN(TestSipHeaders)
#include "tst_sipheaders.moc"
