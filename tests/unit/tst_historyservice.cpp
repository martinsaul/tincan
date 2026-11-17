// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "app/account_service.h"
#include "app/call_service.h"
#include "app/database.h"
#include "app/history_repository.h"
#include "app/history_service.h"
#include "app/secret_store.h"
#include "app/settings_store.h"
#include "fake_sip_engine.h"

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

namespace {

tincan::AccountConfig makeAccount() {
    tincan::AccountConfig acc;
    acc.id = QStringLiteral("acc-1");
    acc.user = QStringLiteral("100");
    acc.domain = QStringLiteral("pbx.example.com");
    return acc;
}

tincan::Database &openMemoryDb(tincan::Database &db) {
    (void)db.open();
    return db;
}

struct HistoryTestRig {
    QTemporaryDir tempDir;
    tincan::SettingsStore settings{tempDir.filePath(QStringLiteral("tincan.ini"))};
    tincan::MemorySecretStore secrets;
    tincan::test::FakeSipEngine engine;
    tincan::AccountService accounts{engine, settings, secrets};
    tincan::CallService calls{engine, accounts, settings};
    tincan::Database db{QStringLiteral(":memory:")};
    tincan::HistoryRepository repo{openMemoryDb(db)};
    tincan::HistoryService history{calls, repo};

    HistoryTestRig() {
        engine.setEvents(&calls);
        (void)accounts.addAccount(makeAccount());
    }
};

} // namespace

class TestHistoryService : public QObject {
    Q_OBJECT

private slots:
    void recordsAnsweredMissedAndAnsweredElsewhere() {
        HistoryTestRig rig;
        QSignalSpy changedSpy(&rig.history, &tincan::HistoryService::historyChanged);
        QSignalSpy missedCountSpy(&rig.history, &tincan::HistoryService::missedCountChanged);

        const int ansId = rig.engine.injectIncomingCall(
            QStringLiteral("acc-1"), QStringLiteral("sip:201@pbx.example.com"),
            QStringLiteral("Alice"));
        QVERIFY(rig.calls.answer(ansId));
        rig.engine.injectCallState(ansId, tincan::CallState::Confirmed, 200);
        rig.engine.injectCallState(ansId, tincan::CallState::Disconnected, 200);
        QCOMPARE(rig.history.unseenMissedCount(), 0);
        QCOMPARE(rig.history.search().first().missed, false);
        QVERIFY(rig.history.search().first().connectedAt.isValid());

        const int missId = rig.engine.injectIncomingCall(
            QStringLiteral("acc-1"), QStringLiteral("sip:202@pbx.example.com"),
            QStringLiteral("Bob"));
        rig.engine.injectCallState(missId, tincan::CallState::Disconnected, 487);
        QCOMPARE(rig.history.unseenMissedCount(), 1);
        QCOMPARE(missedCountSpy.last().first().toInt(), 1);

        const int elsewhereId = rig.engine.injectIncomingCall(
            QStringLiteral("acc-1"), QStringLiteral("sip:203@pbx.example.com"),
            QStringLiteral("Carol"));
        rig.engine.injectCallState(elsewhereId, tincan::CallState::Disconnected, 200,
                                   QStringLiteral("Call completed elsewhere"));
        QCOMPARE(rig.history.unseenMissedCount(), 1);
        QCOMPARE(rig.history.search().first().missed, false);
        QCOMPARE(changedSpy.count(), 3);

        rig.history.markAllSeen();
        QCOMPARE(rig.history.unseenMissedCount(), 0);
        QCOMPARE(missedCountSpy.last().first().toInt(), 0);
    }

    void recordsLocallyRejectedAndOutgoingFailedAsNotMissed() {
        HistoryTestRig rig;

        const int rejId = rig.engine.injectIncomingCall(
            QStringLiteral("acc-1"), QStringLiteral("sip:204@pbx.example.com"),
            QStringLiteral("Dave"));
        QVERIFY(rig.calls.reject(rejId));
        rig.engine.injectCallState(rejId, tincan::CallState::Disconnected, 603);
        QCOMPARE(rig.history.unseenMissedCount(), 0);
        QCOMPARE(rig.history.search().first().missed, false);
        QCOMPARE(rig.history.search().first().statusCode, 603);

        QVERIFY(rig.calls.dial(QStringLiteral("205")).isEmpty());
        const int outId = rig.engine.dialedCalls.last().callId;
        rig.engine.injectCallState(outId, tincan::CallState::Disconnected, 486,
                                   QStringLiteral("Busy Here"));
        const tincan::HistoryEntry lastRow = rig.history.search().first();
        QCOMPARE(lastRow.direction, tincan::CallDirection::Outgoing);
        QVERIFY(!lastRow.connectedAt.isValid());
        QCOMPARE(lastRow.statusCode, 486);
        QCOMPARE(lastRow.missed, false);
    }
};

QTEST_GUILESS_MAIN(TestHistoryService)
#include "tst_historyservice.moc"
