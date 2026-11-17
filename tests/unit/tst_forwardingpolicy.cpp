// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "app/forwarding_policy.h"

#include <QTest>

class TestForwardingPolicy : public QObject {
    Q_OBJECT

private slots:
    void dndRejectsWith486First() {
        tincan::ForwardingSettings cfg;
        cfg.dnd = true;
        cfg.always = QStringLiteral("sip:300@pbx.example.com");

        const auto dec = tincan::ForwardingPolicy::evaluate(cfg, false);
        QCOMPARE(dec.action, tincan::ForwardingAction::Reject486);
    }

    void alwaysRedirectsImmediately() {
        tincan::ForwardingSettings cfg;
        cfg.always = QStringLiteral("  sip:301@pbx.example.com ");
        cfg.onBusy = QStringLiteral("sip:302@pbx.example.com");

        const auto dec = tincan::ForwardingPolicy::evaluate(cfg, true);
        QCOMPARE(dec.action, tincan::ForwardingAction::Redirect);
        QCOMPARE(dec.targetUri, QStringLiteral("sip:301@pbx.example.com"));
    }

    void onBusyRedirectsOnlyWhenBusy() {
        tincan::ForwardingSettings cfg;
        cfg.onBusy = QStringLiteral("sip:busy@pbx.example.com");

        const auto idleDec = tincan::ForwardingPolicy::evaluate(cfg, false);
        QCOMPARE(idleDec.action, tincan::ForwardingAction::Ring);

        const auto busyDec = tincan::ForwardingPolicy::evaluate(cfg, true);
        QCOMPARE(busyDec.action, tincan::ForwardingAction::Redirect);
        QCOMPARE(busyDec.targetUri, QStringLiteral("sip:busy@pbx.example.com"));
    }

    void onNoAnswerReturnsRedirectAfterDelay() {
        tincan::ForwardingSettings cfg;
        cfg.onNoAnswer = QStringLiteral("sip:vm@pbx.example.com");
        cfg.noAnswerSec = 12;

        const auto dec = tincan::ForwardingPolicy::evaluate(cfg, false);
        QCOMPARE(dec.action, tincan::ForwardingAction::RedirectAfter);
        QCOMPARE(dec.targetUri, QStringLiteral("sip:vm@pbx.example.com"));
        QCOMPARE(dec.afterSec, 12);
    }

    void defaultRingsNormally() {
        const tincan::ForwardingSettings cfg;
        const auto dec = tincan::ForwardingPolicy::evaluate(cfg, false);
        QCOMPARE(dec.action, tincan::ForwardingAction::Ring);
    }
};

QTEST_APPLESS_MAIN(TestForwardingPolicy)
#include "tst_forwardingpolicy.moc"
