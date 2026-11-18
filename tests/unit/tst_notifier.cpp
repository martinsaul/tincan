// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "app/account_service.h"
#include "app/call_service.h"
#include "app/dbus_notifications.h"
#include "app/notifier.h"
#include "app/secret_store.h"
#include "app/settings_store.h"
#include "fake_sip_engine.h"

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

namespace {

class FakeNotifications : public tincan::INotificationBackend {
public:
    bool available = true;
    QList<tincan::NotificationRequest> shown;
    QList<uint> closedIds;

    bool isAvailable() const override { return available; }
    uint notify(const tincan::NotificationRequest &request) override {
        shown.append(request);
        return static_cast<uint>(shown.size());
    }
    void close(uint id) override { closedIds.append(id); }
    void invoke(uint id, const QString &key) { emit actionInvoked(id, key); }
};

struct Rig {
    QTemporaryDir dir;
    tincan::SettingsStore settings{dir.filePath(QStringLiteral("tincan.ini"))};
    tincan::MemorySecretStore secrets;
    tincan::test::FakeSipEngine engine;
    tincan::AccountService accounts{engine, settings, secrets};
    tincan::CallService calls{engine, accounts, settings};
    FakeNotifications backend;
    tincan::Notifier notifier{calls, settings, backend};

    Rig() {
        engine.setEvents(&calls);
        tincan::AccountConfig acc;
        acc.id = QStringLiteral("acc");
        acc.user = QStringLiteral("100");
        acc.domain = QStringLiteral("pbx.example.com");
        (void)accounts.addAccount(acc);
    }

    void setAlertMode(tincan::IncomingAlertMode mode) {
        tincan::GeneralSettings general = settings.generalSettings();
        general.incomingAlert = mode;
        settings.setGeneralSettings(general);
    }
};

} // namespace

class TestNotifier : public QObject {
    Q_OBJECT

private slots:
    void incomingCallOffersAnswerAndReject() {
        Rig rig;
        const int callId = rig.engine.injectIncomingCall(QStringLiteral("acc"),
                                                         QStringLiteral("sip:200@pbx.example.com"),
                                                         QStringLiteral("Bob"));
        QCOMPARE(rig.backend.shown.size(), 1);
        const tincan::NotificationRequest &shown = rig.backend.shown.first();
        QCOMPARE(shown.body, QStringLiteral("Bob"));
        QVERIFY(shown.actions.contains(QStringLiteral("answer")));
        QVERIFY(shown.actions.contains(QStringLiteral("reject")));

        rig.backend.invoke(1, QStringLiteral("answer"));
        QCOMPARE(rig.engine.answeredCallIds, QList<int>{callId});
    }

    void rejectActionRejectsAndEndedCallClosesNotification() {
        Rig rig;
        const int callId = rig.engine.injectIncomingCall(QStringLiteral("acc"),
                                                         QStringLiteral("sip:200@pbx"));
        rig.backend.invoke(1, QStringLiteral("reject"));
        QCOMPARE(rig.engine.rejectedCalls.size(), 1);
        rig.engine.injectCallState(callId, tincan::CallState::Disconnected, 486);
        QVERIFY(rig.backend.closedIds.contains(1u));
    }

    void popupOnlyModeSendsNoCallNotification() {
        Rig rig;
        rig.setAlertMode(tincan::IncomingAlertMode::Popup);
        (void)rig.engine.injectIncomingCall(QStringLiteral("acc"), QStringLiteral("sip:200@pbx"));
        QVERIFY(rig.backend.shown.isEmpty());
    }

    void missingServiceFallsBack() {
        Rig rig;
        rig.backend.available = false;
        rig.setAlertMode(tincan::IncomingAlertMode::Notification);
        QSignalSpy fallback(&rig.notifier, &tincan::Notifier::fallbackRequested);
        const int callId = rig.engine.injectIncomingCall(QStringLiteral("acc"),
                                                         QStringLiteral("sip:200@pbx"));
        QCOMPARE(fallback.size(), 1);
        rig.engine.injectCallState(callId, tincan::CallState::Disconnected, 487);
        QVERIFY(fallback.size() >= 2); // missed-call fallback too
        QVERIFY(rig.backend.shown.isEmpty());
    }

    void missedCallOpenRequestsHistory() {
        Rig rig;
        rig.setAlertMode(tincan::IncomingAlertMode::Popup);
        QSignalSpy openHistory(&rig.notifier, &tincan::Notifier::openHistoryRequested);
        const int callId = rig.engine.injectIncomingCall(QStringLiteral("acc"),
                                                         QStringLiteral("sip:200@pbx"));
        rig.engine.injectCallState(callId, tincan::CallState::Disconnected, 487);
        QCOMPARE(rig.backend.shown.size(), 1);
        QVERIFY(rig.backend.shown.first().actions.contains(QStringLiteral("open")));
        rig.backend.invoke(1, QStringLiteral("open"));
        QCOMPARE(openHistory.size(), 1);
    }

    // Remote display names, URIs and message bodies are caller-controlled and
    // must not be rendered as markup by servers with "body-markup".
    void dbusBodyEscapesMarkupForMarkupServers() {
        const QString hostile =
            QStringLiteral("<a href=\"https://x.example\">Bob</a> & <img src=x>");
        const QString escaped = tincan::DbusNotifications::bodyFor(hostile, true);
        QVERIFY(!escaped.contains(u'<'));
        QVERIFY(!escaped.contains(u'>'));
        QCOMPARE(escaped, QStringLiteral("&lt;a href=&quot;https://x.example&quot;&gt;Bob&lt;/a&gt;"
                                         " &amp; &lt;img src=x&gt;"));
        QCOMPARE(tincan::DbusNotifications::bodyFor(hostile, false), hostile);
    }
};

QTEST_GUILESS_MAIN(TestNotifier)
#include "tst_notifier.moc"
