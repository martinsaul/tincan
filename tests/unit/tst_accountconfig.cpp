// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "core/account_config.h"

#include <QTest>

class TestAccountConfig : public QObject {
    Q_OBJECT

private slots:
    void formatsIdUriWithAndWithoutDisplayName() {
        tincan::AccountConfig cfg;
        cfg.user = QStringLiteral("alice");
        cfg.domain = QStringLiteral("sip.example.com");
        QCOMPARE(cfg.idUri(), QStringLiteral("sip:alice@sip.example.com"));

        cfg.displayName = QStringLiteral("Alice Smith");
        QCOMPARE(cfg.idUri(), QStringLiteral("\"Alice Smith\" <sip:alice@sip.example.com>"));

        cfg.displayName = QStringLiteral("Alice \"AJ\"");
        QCOMPARE(cfg.idUri(), QStringLiteral("\"Alice \\\"AJ\\\"\" <sip:alice@sip.example.com>"));
    }

    void formatsIdUriForDirectIpAndEmptyDomain() {
        tincan::AccountConfig direct;
        direct.domain = QStringLiteral("127.0.0.1");
        QCOMPARE(direct.idUri(), QStringLiteral("sip:127.0.0.1"));

        tincan::AccountConfig empty;
        empty.user = QStringLiteral("alice");
        QVERIFY(empty.idUri().isEmpty());
    }

    void formatsRegistrarUriAcrossTransports() {
        tincan::AccountConfig cfg;
        cfg.user = QStringLiteral("alice");
        cfg.domain = QStringLiteral("sip.example.com:5060");

        cfg.transport = tincan::Transport::Udp;
        QCOMPARE(cfg.registrarUri(), QStringLiteral("sip:sip.example.com:5060"));

        cfg.transport = tincan::Transport::Tcp;
        QCOMPARE(cfg.registrarUri(), QStringLiteral("sip:sip.example.com:5060;transport=tcp"));

        cfg.transport = tincan::Transport::Tls;
        QCOMPARE(cfg.registrarUri(), QStringLiteral("sip:sip.example.com:5060;transport=tls"));
    }

    void omitsRegistrarUriWhenDisabledOrDomainEmpty() {
        tincan::AccountConfig cfg;
        cfg.user = QStringLiteral("tincan");
        cfg.domain = QStringLiteral("127.0.0.1");
        cfg.registerAccount = false;
        QVERIFY(cfg.registrarUri().isEmpty());

        tincan::AccountConfig noDomain;
        noDomain.registerAccount = true;
        QVERIFY(noDomain.registrarUri().isEmpty());
    }
};

QTEST_APPLESS_MAIN(TestAccountConfig)
#include "tst_accountconfig.moc"
