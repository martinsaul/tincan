// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "app/account_service.h"
#include "app/audio_service.h"
#include "app/call_service.h"
#include "app/contact_repository.h"
#include "app/contact_service.h"
#include "app/database.h"
#include "app/history_repository.h"
#include "app/history_service.h"
#include "app/message_repository.h"
#include "app/message_service.h"
#include "app/secret_store.h"
#include "app/settings_store.h"
#include "app/sip_event_router.h"
#include "fake_sip_engine.h"
#include "ui/about_dialog.h"
#include "ui/call_card.h"
#include "ui/call_panel.h"
#include "ui/main_window.h"

#include <QAction>
#include <QApplication>
#include <QPushButton>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

namespace {

tincan::Database &openMemoryDb(tincan::Database &db) {
    (void)db.open();
    return db;
}

struct P6Rig {
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

    P6Rig() {
        router.addListener(&calls);
        router.addListener(&contacts);
        router.addListener(&messages);
    }

    std::unique_ptr<tincan::ui::MainWindow> makeWindow() {
        return std::make_unique<tincan::ui::MainWindow>(accounts, calls, audio, contacts, history,
                                                        messages, settings, secrets);
    }
};

tincan::CallInfo connectedCall(int callId) {
    tincan::CallInfo info;
    info.callId = callId;
    info.accountId = QStringLiteral("acc-1");
    info.direction = tincan::CallDirection::Outgoing;
    info.state = tincan::CallState::Confirmed;
    info.remoteUri = QStringLiteral("sip:204@pbx.example.net");
    return info;
}

} // namespace

class TestUiP6 : public QObject {
    Q_OBJECT

private slots:
    void inCallKeypadCollapsesMainTabs() {
        P6Rig rig;
        auto window = rig.makeWindow();
        window->show();
        tincan::CallInfo call = connectedCall(7);
        rig.calls.onCallChanged(call);
        tincan::ui::CallCard *card = window->callPanel()->cardForCall(7);
        QVERIFY(card != nullptr);
        QVERIFY(window->tabs()->isVisible());

        card->keypadButton()->click();
        QVERIFY(window->callPanel()->isInCallKeypadShown());
        QVERIFY(!window->tabs()->isVisible());

        card->keypadButton()->click();
        QVERIFY(window->tabs()->isVisible());

        card->keypadButton()->click();
        QVERIFY(!window->tabs()->isVisible());
        call.state = tincan::CallState::Disconnected;
        rig.calls.onCallChanged(call);
        QTRY_VERIFY(window->tabs()->isVisible());
        QVERIFY(!window->callPanel()->isInCallKeypadShown());
    }

    void windowRemembersGeometryAndLastTab() {
        P6Rig rig;
        {
            auto window = rig.makeWindow();
            window->resize(410, 620);
            window->show();
            QVERIFY(QTest::qWaitForWindowExposed(window.get()));
            window->tabs()->setCurrentIndex(2);
        }
        QVERIFY(!rig.settings.windowState().geometry.isEmpty());
        QCOMPARE(rig.settings.windowState().lastTab, 2);

        auto restored = rig.makeWindow();
        QCOMPARE(restored->tabs()->currentIndex(), 2);
        QCOMPARE(restored->size(), QSize(410, 620));
    }

    void aboutDialogShowsVersionLicenseAndCredits() {
        P6Rig rig;
        auto window = rig.makeWindow();
        QVERIFY(window->aboutAction() != nullptr);

        const tincan::ui::AboutDialog dlg(QStringLiteral("2.17"));
        const QString html = dlg.bodyHtml();
        QVERIFY(html.contains(QStringLiteral("Tincan 0.1.1")));
        QVERIFY(html.contains(QStringLiteral("PJSIP 2.17")));
        QVERIFY(html.contains(QStringLiteral("AGPL-3.0-or-later")));
        QVERIFY(html.contains(QStringLiteral("https://github.com/martinsaul/tincan")));
        QVERIFY(html.contains(QStringLiteral("QtKeychain")));
        QVERIFY(html.contains(QStringLiteral("Opus")));
        QVERIFY(html.contains(QStringLiteral("https://www.microsip.org")));
        QVERIFY(html.contains(QStringLiteral("not affiliated")));
    }
};

QTEST_MAIN(TestUiP6)
#include "tst_ui_p6.moc"
