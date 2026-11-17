// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "app/auto_answer_policy.h"

#include <QTest>

namespace {

tincan::CallInfo makeIncomingRingingCall(const QMap<QString, QString> &headers = {}) {
    tincan::CallInfo call;
    call.callId = 1;
    call.direction = tincan::CallDirection::Incoming;
    call.state = tincan::CallState::Incoming;
    call.remoteUri = QStringLiteral("sip:200@pbx.example.com");
    call.incomingHeaders = headers;
    return call;
}

} // namespace

class TestAutoAnswerPolicy : public QObject {
    Q_OBJECT

private slots:
    void offNeverAutoAnswers() {
        const tincan::AutoAnswerSettings cfg{tincan::AutoAnswerMode::Off, 0};
        const auto call = makeIncomingRingingCall(
            {{QStringLiteral("Call-Info"), QStringLiteral("<sip:x>;answer-after=0")}});
        const auto dec = tincan::AutoAnswerPolicy::evaluate(cfg, call, false);
        QVERIFY(!dec.shouldAnswer);
    }

    void alwaysModeUsesConfiguredDelayUnlessBusy() {
        const tincan::AutoAnswerSettings cfg{tincan::AutoAnswerMode::Always, 3};
        const auto call = makeIncomingRingingCall();

        const auto idleDec = tincan::AutoAnswerPolicy::evaluate(cfg, call, false);
        QVERIFY(idleDec.shouldAnswer);
        QCOMPARE(idleDec.delaySec, 3);

        const auto busyDec = tincan::AutoAnswerPolicy::evaluate(cfg, call, true);
        QVERIFY(!busyDec.shouldAnswer);
    }

    void headerOnlyMatchesCallInfoAnswerAfterDelay() {
        const tincan::AutoAnswerSettings cfg{tincan::AutoAnswerMode::HeaderOnly, 5};
        const auto call = makeIncomingRingingCall(
            {{QStringLiteral("Call-Info"), QStringLiteral("<sip:pbx>;answer-after=2")}});

        const auto dec = tincan::AutoAnswerPolicy::evaluate(cfg, call, false);
        QVERIFY(dec.shouldAnswer);
        QCOMPARE(dec.delaySec, 2);

        const auto busyDec = tincan::AutoAnswerPolicy::evaluate(cfg, call, true);
        QVERIFY(!busyDec.shouldAnswer);
    }

    void headerOnlyMatchesAlertInfoAndAnswerMode() {
        const tincan::AutoAnswerSettings cfg{tincan::AutoAnswerMode::HeaderOnly, 1};

        const auto alertAuto = makeIncomingRingingCall(
            {{QStringLiteral("Alert-Info"),
              QStringLiteral("<http://127.0.0.1>;info=alert-autoanswer")}});
        const auto d1 = tincan::AutoAnswerPolicy::evaluate(cfg, alertAuto, false);
        QVERIFY(d1.shouldAnswer);
        QCOMPARE(d1.delaySec, 1);

        const auto intercom = makeIncomingRingingCall(
            {{QStringLiteral("Alert-Info"), QStringLiteral("<sip:page>;info=intercom")}});
        const auto d2 = tincan::AutoAnswerPolicy::evaluate(cfg, intercom, false);
        QVERIFY(d2.shouldAnswer);
        QCOMPARE(d2.delaySec, 1);

        const auto answerMode = makeIncomingRingingCall(
            {{QStringLiteral("Answer-Mode"), QStringLiteral("Auto;require")}});
        const auto d3 = tincan::AutoAnswerPolicy::evaluate(cfg, answerMode, false);
        QVERIFY(d3.shouldAnswer);
        QCOMPARE(d3.delaySec, 1);

        const auto manualMode =
            makeIncomingRingingCall({{QStringLiteral("Answer-Mode"), QStringLiteral("Manual")}});
        QVERIFY(!tincan::AutoAnswerPolicy::evaluate(cfg, manualMode, false).shouldAnswer);

        const auto plainCall = makeIncomingRingingCall();
        QVERIFY(!tincan::AutoAnswerPolicy::evaluate(cfg, plainCall, false).shouldAnswer);
    }
};

QTEST_APPLESS_MAIN(TestAutoAnswerPolicy)
#include "tst_autoanswerpolicy.moc"
