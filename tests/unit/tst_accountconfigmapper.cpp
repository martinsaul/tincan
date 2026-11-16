// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "core/account_config.h"
#include "sip/account_config_mapper.h"

#include <QTest>

class TestAccountConfigMapper : public QObject {
    Q_OBJECT

private slots:
    void mapsRegisteredAccountFields() {
        tincan::AccountConfig cfg;
        cfg.id = QStringLiteral("work");
        cfg.displayName = QStringLiteral("Alice");
        cfg.user = QStringLiteral("1001");
        cfg.domain = QStringLiteral("pbx.example.com");
        cfg.password = QStringLiteral("s3cr3t");
        cfg.proxy = QStringLiteral("sip:edge.example.com;lr");
        cfg.transport = tincan::Transport::Tls;
        cfg.srtp = tincan::SrtpMode::Mandatory;
        cfg.registerAccount = true;
        cfg.regExpirySec = 600;
        cfg.stunServer = QStringLiteral("stun.example.com:3478");
        cfg.ice = true;
        cfg.publicAddress = QStringLiteral("203.0.113.10");

        const pj::AccountConfig pjCfg = tincan::sip::AccountConfigMapper::map(cfg);
        QCOMPARE(QString::fromStdString(pjCfg.idUri),
                 QStringLiteral("\"Alice\" <sip:1001@pbx.example.com>"));
        QCOMPARE(QString::fromStdString(pjCfg.regConfig.registrarUri),
                 QStringLiteral("sip:pbx.example.com;transport=tls"));
        QVERIFY(pjCfg.regConfig.registerOnAdd);
        QCOMPARE(pjCfg.regConfig.timeoutSec, 600u);

        QCOMPARE(pjCfg.sipConfig.authCreds.size(), std::size_t{1});
        QCOMPARE(pjCfg.sipConfig.authCreds[0].scheme, std::string("digest"));
        QCOMPARE(pjCfg.sipConfig.authCreds[0].realm, std::string("*"));
        QCOMPARE(pjCfg.sipConfig.authCreds[0].username, std::string("1001"));
        QCOMPARE(pjCfg.sipConfig.authCreds[0].dataType, 0);
        QCOMPARE(pjCfg.sipConfig.authCreds[0].data, std::string("s3cr3t"));

        QCOMPARE(pjCfg.sipConfig.proxies.size(), std::size_t{1});
        QCOMPARE(pjCfg.sipConfig.proxies[0], std::string("sip:edge.example.com;lr"));

        QVERIFY(pjCfg.natConfig.iceEnabled);
        QCOMPARE(pjCfg.natConfig.sipStunUse, PJSUA_STUN_USE_DEFAULT);
        QCOMPARE(pjCfg.natConfig.mediaStunUse, PJSUA_STUN_USE_DEFAULT);
        QCOMPARE(pjCfg.mediaConfig.srtpUse, PJMEDIA_SRTP_MANDATORY);
        QCOMPARE(pjCfg.mediaConfig.srtpSecureSignaling, 0);
        QCOMPARE(pjCfg.mediaConfig.transportConfig.publicAddress, std::string("203.0.113.10"));
    }

    void mapsExplicitAuthUserAndOptionalSrtp() {
        tincan::AccountConfig cfg;
        cfg.user = QStringLiteral("1001");
        cfg.authUser = QStringLiteral("auth-1001");
        cfg.domain = QStringLiteral("pbx.example.com");
        cfg.password = QStringLiteral("pw");
        cfg.srtp = tincan::SrtpMode::Optional;

        const pj::AccountConfig pjCfg = tincan::sip::AccountConfigMapper::map(cfg);
        QCOMPARE(pjCfg.sipConfig.authCreds.size(), std::size_t{1});
        QCOMPARE(pjCfg.sipConfig.authCreds[0].username, std::string("auth-1001"));
        QCOMPARE(pjCfg.mediaConfig.srtpUse, PJMEDIA_SRTP_OPTIONAL);
        QCOMPARE(pjCfg.mediaConfig.srtpSecureSignaling, 0);
        QCOMPARE(pjCfg.natConfig.sipStunUse, PJSUA_STUN_USE_DISABLED);
    }

    void mapsDirectIpAccountWithoutRegistrar() {
        tincan::AccountConfig local;
        local.id = QStringLiteral("local");
        local.user = QStringLiteral("tincan");
        local.domain = QStringLiteral("127.0.0.1");
        local.registerAccount = false;

        const pj::AccountConfig pjCfg = tincan::sip::AccountConfigMapper::map(local);
        QCOMPARE(pjCfg.idUri, std::string("sip:tincan@127.0.0.1"));
        QVERIFY(pjCfg.regConfig.registrarUri.empty());
        QVERIFY(!pjCfg.regConfig.registerOnAdd);
        QVERIFY(pjCfg.sipConfig.authCreds.empty());
        QCOMPARE(pjCfg.mediaConfig.srtpUse, PJMEDIA_SRTP_DISABLED);

        tincan::AccountConfig emptyDomain;
        emptyDomain.user = QStringLiteral("peer");
        const pj::AccountConfig fallbackCfg = tincan::sip::AccountConfigMapper::map(emptyDomain);
        QCOMPARE(fallbackCfg.idUri, std::string("sip:peer@127.0.0.1"));
        QVERIFY(fallbackCfg.regConfig.registrarUri.empty());
        QVERIFY(!fallbackCfg.regConfig.registerOnAdd);
    }
};

QTEST_APPLESS_MAIN(TestAccountConfigMapper)
#include "tst_accountconfigmapper.moc"
