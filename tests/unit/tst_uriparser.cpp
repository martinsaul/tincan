// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "app/uri_parser.h"

#include <QTest>

using tincan::UriParser::hasSupportedScheme;
using tincan::UriParser::parse;

class TestUriParser : public QObject {
    Q_OBJECT

private slots:
    void parsesUris_data() {
        QTest::addColumn<QString>("input");
        QTest::addColumn<QString>("expected");

        // tel:
        QTest::newRow("tel global") << "tel:+1-206-555-0100;phone-context=example.com"
                                    << "+12065550100";
        QTest::newRow("tel local") << "tel:555.0100" << "5550100";
        QTest::newRow("tel uppercase scheme") << "TEL:+44 20 7946 0958" << "+442079460958";
        QTest::newRow("tel percent-encoded") << "tel:%2B1%20(206)%20555-0100" << "+12065550100";
        QTest::newRow("tel star hash") << "tel:*67#" << "*67#";
        QTest::newRow("tel header stripped") << "tel:100?foo=bar" << "100";

        // callto:
        QTest::newRow("callto slashes") << "callto://alice" << "alice";
        QTest::newRow("callto plain") << "callto:alice" << "alice";
        QTest::newRow("callto trailing slash") << "callto://alice/" << "alice";
        QTest::newRow("callto number") << "callto:+1%20(206)%20555-0100" << "+12065550100";
        QTest::newRow("callto user@host") << "callto:bob@example.com" << "bob@example.com";
        QTest::newRow("callto encoded") << "callto://al%69ce" << "alice";

        // sip: / sips:
        QTest::newRow("sip plain") << "sip:100@pbx.example.com" << "sip:100@pbx.example.com";
        QTest::newRow("sip transport kept")
            << "sip:100@pbx.example.com;transport=tcp" << "sip:100@pbx.example.com;transport=tcp";
        QTest::newRow("sip headers stripped")
            << "sip:100@pbx.example.com?subject=hi%20there" << "sip:100@pbx.example.com";
        QTest::newRow("sips") << "sips:alice@example.com:5061" << "sips:alice@example.com:5061";
        QTest::newRow("sip host only") << "sip:pbx.example.com" << "sip:pbx.example.com";
        QTest::newRow("sip ipv4 port") << "sip:123@192.0.2.10:5070" << "sip:123@192.0.2.10:5070";
        QTest::newRow("sip ipv6") << "sip:123@[2001:db8::1]:5060" << "sip:123@[2001:db8::1]:5060";
        QTest::newRow("sip percent user") << "sip:%31%30%30@host" << "sip:100@host";
        QTest::newRow("sip scheme case") << "SIP:100@host" << "sip:100@host";
        QTest::newRow("sip whitespace trimmed") << "  sip:100@host \n" << "sip:100@host";
    }

    void parsesUris() {
        QFETCH(QString, input);
        QFETCH(QString, expected);
        QCOMPARE(parse(input), expected);
    }

    void rejectsGarbage_data() {
        QTest::addColumn<QString>("input");
        QTest::newRow("empty") << "";
        QTest::newRow("http") << "http://example.com";
        QTest::newRow("mailto") << "mailto:alice@example.com";
        QTest::newRow("bare number") << "100";
        QTest::newRow("tel empty") << "tel:";
        QTest::newRow("tel letters") << "tel:call-me";
        QTest::newRow("tel bad percent") << "tel:%zz12";
        QTest::newRow("tel truncated percent") << "tel:12%2";
        QTest::newRow("tel control char") << "tel:12%0a34";
        QTest::newRow("callto empty") << "callto://";
        QTest::newRow("callto path") << "callto://alice/bob";
        QTest::newRow("callto space") << "callto:al ice";
        QTest::newRow("sip empty") << "sip:";
        QTest::newRow("sip only user") << "sip:100@";
        QTest::newRow("sip two ats") << "sip:a@b@c";
        QTest::newRow("sip space") << "sip:100 @host";
        QTest::newRow("sip angle") << "sip:<100@host>";
        QTest::newRow("sip bad port") << "sip:100@host:99999";
        QTest::newRow("sip bad host") << "sip:100@-host";
        QTest::newRow("sip invalid utf8") << "sip:%ff%fe@host";
        QTest::newRow("sip encoded newline") << "sip:100@host%0d%0aVia:x";
        QTest::newRow("sip bad header encoding") << "sip:100@host?x=%g1";
        QTest::newRow("sip encoded markup user") << "sip:%3Cb%3Ex%3C/b%3E@host";
        QTest::newRow("sip encoded nul") << "sip:100%00@host";
        QTest::newRow("sip encoded at in host") << "sip:100@host%40evil";
        QTest::newRow("sip markup param") << "sip:100@host;x=%3Cimg%3E";
        QTest::newRow("callto encoded markup") << "callto:%3Ca%20href%3D1%3E";
        QTest::newRow("tel fullwidth digits") << QStringLiteral("tel:\uFF11\uFF12");
        QTest::newRow("oversized") << QStringLiteral("sip:") + QString(4096, u'1') +
                                          QStringLiteral("@host");
    }

    void rejectsGarbage() {
        QFETCH(QString, input);
        QVERIFY2(parse(input).isEmpty(), qPrintable(parse(input)));
    }

    void detectsSupportedSchemes() {
        QVERIFY(hasSupportedScheme(QStringLiteral("sip:1@h")));
        QVERIFY(hasSupportedScheme(QStringLiteral("SIPS:1@h")));
        QVERIFY(hasSupportedScheme(QStringLiteral("tel:1")));
        QVERIFY(hasSupportedScheme(QStringLiteral("callto:x")));
        QVERIFY(!hasSupportedScheme(QStringLiteral("http://x")));
        QVERIFY(!hasSupportedScheme(QStringLiteral("100")));
    }
};

QTEST_GUILESS_MAIN(TestUriParser)
#include "tst_uriparser.moc"
