// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "app/account_service.h"
#include "app/call_service.h"
#include "app/command_dispatcher.h"
#include "app/secret_store.h"
#include "app/settings_store.h"
#include "fake_sip_engine.h"

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using tincan::CommandLine;

namespace {

CommandLine command(CommandLine::Action action, const QString &target = {}) {
    CommandLine cmd;
    cmd.action = action;
    cmd.target = target;
    return cmd;
}

struct Rig {
    QTemporaryDir dir;
    tincan::SettingsStore settings{dir.filePath(QStringLiteral("tincan.ini"))};
    tincan::MemorySecretStore secrets;
    tincan::test::FakeSipEngine engine;
    tincan::AccountService accounts{engine, settings, secrets};
    tincan::CallService calls{engine, accounts, settings};
    tincan::CommandDispatcher dispatcher{calls};

    Rig() {
        engine.setEvents(&calls);
        tincan::AccountConfig acc;
        acc.id = QStringLiteral("acc");
        acc.user = QStringLiteral("100");
        acc.domain = QStringLiteral("pbx.example.com");
        (void)accounts.addAccount(acc);
    }
};

} // namespace

class TestCommandDispatcher : public QObject {
    Q_OBJECT

private slots:
    void dialLinkPrefillsButNeverCalls() {
        Rig rig;
        QSignalSpy shown(&rig.dispatcher, &tincan::CommandDispatcher::showRequested);
        QSignalSpy prefill(&rig.dispatcher, &tincan::CommandDispatcher::dialPrefillRequested);
        QVERIFY(rig.dispatcher.dispatch(command(CommandLine::Action::Dial,
                                                QStringLiteral("sip:123@host"))));
        QVERIFY(rig.engine.dialedCalls.isEmpty());
        QCOMPARE(prefill.size(), 1);
        QCOMPARE(prefill.first().first().toString(), QStringLiteral("sip:123@host"));
        QCOMPARE(shown.size(), 1);
    }

    void answerPicksFirstRingingCall() {
        Rig rig;
        QVERIFY(!rig.dispatcher.dispatch(command(CommandLine::Action::Answer)));
        const int first = rig.engine.injectIncomingCall(QStringLiteral("acc"),
                                                        QStringLiteral("sip:200@pbx"));
        (void)rig.engine.injectIncomingCall(QStringLiteral("acc"), QStringLiteral("sip:201@pbx"));
        QVERIFY(rig.dispatcher.dispatch(command(CommandLine::Action::Answer)));
        QCOMPARE(rig.engine.answeredCallIds, QList<int>{first});
    }

    void hangupEndsActiveCall() {
        Rig rig;
        QVERIFY(!rig.dispatcher.dispatch(command(CommandLine::Action::Hangup)));
        QVERIFY(rig.calls.dial(QStringLiteral("300")).isEmpty());
        const int callId = rig.engine.dialedCalls.first().callId;
        rig.engine.injectCallState(callId, tincan::CallState::Confirmed, 200);
        QVERIFY(rig.dispatcher.dispatch(command(CommandLine::Action::Hangup)));
        QVERIFY(rig.engine.hungupCallIds.contains(callId));
    }

    void showAndQuitAreForwardedAsSignals() {
        Rig rig;
        QSignalSpy shown(&rig.dispatcher, &tincan::CommandDispatcher::showRequested);
        QSignalSpy quit(&rig.dispatcher, &tincan::CommandDispatcher::quitRequested);
        QVERIFY(rig.dispatcher.dispatch(command(CommandLine::Action::Show)));
        QVERIFY(rig.dispatcher.dispatch(command(CommandLine::Action::Quit)));
        QCOMPARE(shown.size(), 1);
        QCOMPARE(quit.size(), 1);
    }
};

QTEST_GUILESS_MAIN(TestCommandDispatcher)
#include "tst_commanddispatcher.moc"
