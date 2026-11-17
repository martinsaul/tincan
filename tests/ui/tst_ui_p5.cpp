// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "app/account_service.h"
#include "app/audio_service.h"
#include "app/call_service.h"
#include "app/command_dispatcher.h"
#include "app/contact_repository.h"
#include "app/contact_service.h"
#include "app/database.h"
#include "app/history_repository.h"
#include "app/history_service.h"
#include "app/message_repository.h"
#include "app/message_service.h"
#include "app/secret_store.h"
#include "app/settings_store.h"
#include "app/single_instance.h"
#include "app/sip_event_router.h"
#include "fake_sip_engine.h"
#include "ui/general_page.h"
#include "ui/incoming_call_popup.h"
#include "ui/main_window.h"

#include <QApplication>
#include <QComboBox>
#include <QTemporaryDir>
#include <QTest>

namespace {

tincan::Database &openMemoryDb(tincan::Database &db) {
    (void)db.open();
    return db;
}

struct P5Rig {
    QTemporaryDir tempDir;
    tincan::SettingsStore settings{tempDir.filePath(QStringLiteral("tincan.ini"))};
    tincan::MemorySecretStore secrets;
    tincan::SipEventRouter router;
    tincan::test::FakeSipEngine engine{&router};
    tincan::Database db{QStringLiteral(":memory:")};
    tincan::ContactRepository contactRepo{openMemoryDb(db)};
    tincan::HistoryRepository historyRepo{db};
    tincan::MessageRepository messageRepo{db};
    tincan::AccountService accounts{engine, settings, secrets};
    tincan::CallService calls{engine, accounts, settings};
    tincan::AudioService audio{engine, settings};
    tincan::ContactService contacts{engine, accounts, contactRepo};
    tincan::HistoryService history{calls, historyRepo, &contacts};
    tincan::MessageService messages{engine, accounts, messageRepo};
    tincan::ui::MainWindow window{accounts, calls,    audio,    contacts,
                                  history,  messages, settings, secrets};

    P5Rig() {
        router.addListener(&calls);
        router.addListener(&contacts);
        router.addListener(&messages);
        tincan::AccountConfig acc;
        acc.id = QStringLiteral("acc-1");
        acc.user = QStringLiteral("100");
        acc.domain = QStringLiteral("pbx.example.net");
        (void)accounts.addAccount(acc);
    }
};

} // namespace

class TestUiP5 : public QObject {
    Q_OBJECT

private slots:
    void forwardedSipUriDialsThroughPrimary() {
        P5Rig rig;
        QVERIFY(rig.tempDir.isValid());
        tincan::CommandDispatcher dispatcher(rig.calls);
        connect(&dispatcher, &tincan::CommandDispatcher::showRequested, &rig.window,
                &tincan::ui::MainWindow::bringToFront);

        const QString server = rig.tempDir.filePath(QStringLiteral("p5.sock"));
        tincan::SingleInstance primary(server);
        QCOMPARE(primary.start(tincan::CommandLine{}), tincan::SingleInstance::Role::Primary);
        connect(&primary, &tincan::SingleInstance::commandReceived, &dispatcher,
                &tincan::CommandDispatcher::dispatch);

        const tincan::CommandLineParseResult parsed = tincan::parseCommandLine(
            {QStringLiteral("tincan"), QStringLiteral("sip:123@host")});
        QVERIFY(parsed.ok);
        tincan::SingleInstance secondary(server);
        QCOMPARE(secondary.start(parsed.command), tincan::SingleInstance::Role::Forwarded);

        QTRY_COMPARE(rig.engine.dialedCalls.size(), 1);
        QCOMPARE(rig.engine.dialedCalls.first().uri, QStringLiteral("sip:123@host"));
        QCOMPARE(rig.engine.dialedCalls.first().accountId, QStringLiteral("acc-1"));
        QTRY_VERIFY(rig.window.isVisible());
    }

    void generalPagePersistsIncomingAlertMode() {
        P5Rig rig;
        tincan::ui::GeneralPage page(rig.settings, rig.audio);
        auto *combo = page.findChild<QComboBox *>(QStringLiteral("incomingAlertCombo"));
        QVERIFY(combo != nullptr);
        QCOMPARE(combo->currentData().toInt(),
                 static_cast<int>(tincan::IncomingAlertMode::Both));
        combo->setCurrentIndex(
            combo->findData(static_cast<int>(tincan::IncomingAlertMode::Notification)));
        page.applyChanges();
        QCOMPARE(rig.settings.generalSettings().incomingAlert,
                 tincan::IncomingAlertMode::Notification);
    }

    void popupRespectsShowPredicate() {
        P5Rig rig;
        tincan::ui::IncomingCallPopup popup(rig.calls, &rig.contacts);
        bool allowPopup = false;
        popup.setShowPredicate([&allowPopup]() { return allowPopup; });
        (void)rig.engine.injectIncomingCall(QStringLiteral("acc-1"),
                                            QStringLiteral("sip:200@pbx.example.net"));
        QCoreApplication::processEvents();
        QVERIFY(!popup.isVisible());

        allowPopup = true;
        (void)rig.engine.injectIncomingCall(QStringLiteral("acc-1"),
                                            QStringLiteral("sip:201@pbx.example.net"));
        QCoreApplication::processEvents();
        QVERIFY(popup.isVisible());
    }
};

QTEST_MAIN(TestUiP5)
#include "tst_ui_p5.moc"
