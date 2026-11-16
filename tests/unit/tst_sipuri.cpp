// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "core/account_config.h"
#include "core/sip_uri.h"

#include <QTest>

namespace {

tincan::AccountConfig sampleAccount(const QString &prefix = {}) {
    tincan::AccountConfig cfg;
    cfg.id = QStringLiteral("acc-1");
    cfg.user = QStringLiteral("1000");
    cfg.domain = QStringLiteral("pbx.example.com");
    cfg.dialPrefix = prefix;
    return cfg;
}

} // namespace

class TestSipUri : public QObject {
    Q_OBJECT

private slots:
    void normalizesFormattingCharacters() {
        QCOMPARE(tincan::SipUri::normalizeDialString(QStringLiteral(" (555) 123-45.67 ")),
                 QStringLiteral("5551234567"));
        QCOMPARE(tincan::SipUri::normalizeDialString(QStringLiteral("[020] 7946-0958")),
                 QStringLiteral("02079460958"));
    }

    void keepsLeadingPlusDigitsStarHashAndLetters() {
        QCOMPARE(tincan::SipUri::normalizeDialString(QStringLiteral("+1 (800) 555-0199")),
                 QStringLiteral("+18005550199"));
        QCOMPARE(tincan::SipUri::normalizeDialString(QStringLiteral("12+34")),
                 QStringLiteral("1234"));
        QCOMPARE(tincan::SipUri::normalizeDialString(QStringLiteral("*97#")),
                 QStringLiteral("*97#"));
        QCOMPARE(tincan::SipUri::normalizeDialString(QStringLiteral("alice_ext")),
                 QStringLiteral("alice_ext"));
    }

    void buildsUriForPlainNumberAndE164() {
        const auto acc = sampleAccount();
        QCOMPARE(tincan::SipUri::toSipUri(QStringLiteral("555-1234"), acc),
                 QStringLiteral("sip:5551234@pbx.example.com"));
        QCOMPARE(tincan::SipUri::toSipUri(QStringLiteral("+1 (555) 123-4567"), acc),
                 QStringLiteral("sip:+15551234567@pbx.example.com"));
    }

    void appliesDialPrefixToPlainNumbersOnly() {
        const auto acc = sampleAccount(QStringLiteral("9"));
        QCOMPARE(tincan::SipUri::toSipUri(QStringLiteral("555.1234"), acc),
                 QStringLiteral("sip:95551234@pbx.example.com"));
        QCOMPARE(tincan::SipUri::toSipUri(QStringLiteral("bob@other.example.org:5080"), acc),
                 QStringLiteral("sip:bob@other.example.org:5080"));
    }

    void preservesExistingSipAndSipsUris() {
        const auto acc = sampleAccount(QStringLiteral("9"));
        QCOMPARE(tincan::SipUri::toSipUri(QStringLiteral("sip:200@peer.local:5060"), acc),
                 QStringLiteral("sip:200@peer.local:5060"));
        QCOMPARE(tincan::SipUri::toSipUri(QStringLiteral("sips:secure@peer.local"), acc),
                 QStringLiteral("sips:secure@peer.local"));
    }

    void returnsEmptyForBlankOrUnusableInput() {
        const auto acc = sampleAccount();
        QVERIFY(tincan::SipUri::toSipUri(QString{}, acc).isEmpty());
        QVERIFY(tincan::SipUri::toSipUri(QStringLiteral("   --- ()  "), acc).isEmpty());

        tincan::AccountConfig noDomain;
        QVERIFY(tincan::SipUri::toSipUri(QStringLiteral("100"), noDomain).isEmpty());
    }
};

QTEST_APPLESS_MAIN(TestSipUri)
#include "tst_sipuri.moc"
