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
#include "ui/call_card.h"
#include "ui/call_panel.h"
#include "ui/chat_window.h"
#include "ui/contact_dialog.h"
#include "ui/contacts_view.h"
#include "ui/history_view.h"
#include "ui/incoming_call_popup.h"
#include "ui/main_window.h"
#include "ui/messages_view.h"
#include "ui/tray_icon.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

namespace {

tincan::AccountConfig sampleOfficeAccount(bool publishPresence = true) {
    tincan::AccountConfig acc;
    acc.id = QStringLiteral("acc-1");
    acc.displayName = QStringLiteral("Main Office");
    acc.user = QStringLiteral("100");
    acc.domain = QStringLiteral("pbx.example.net");
    acc.publishPresence = publishPresence;
    return acc;
}

tincan::Database &openMemoryDb(tincan::Database &db) {
    (void)db.open();
    return db;
}

struct P4UiRig {
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

    P4UiRig() {
        router.addListener(&calls);
        router.addListener(&contacts);
        router.addListener(&messages);
    }

    std::unique_ptr<tincan::ui::MainWindow> makeWindow() {
        return std::make_unique<tincan::ui::MainWindow>(accounts, calls, audio, contacts, history,
                                                        messages, settings, secrets);
    }
};

} // namespace

class TestUiP4 : public QObject {
    Q_OBJECT

private slots:
    void contactsAddViaDialogAppearsAndDoubleClickDialsViaFakeEngine() {
        P4UiRig rig;
        QVERIFY(rig.accounts.addAccount(sampleOfficeAccount()));

        auto window = rig.makeWindow();
        window->show();
        QVERIFY(QTest::qWaitForWindowExposed(window.get()));

        window->tabs()->setCurrentIndex(1);
        QCoreApplication::processEvents();

        QTimer::singleShot(0, window.get(), []() {
            auto *dlg =
                qobject_cast<tincan::ui::ContactDialog *>(QApplication::activeModalWidget());
            QVERIFY(dlg != nullptr);
            dlg->findChild<QLineEdit *>(QStringLiteral("contactNameEdit"))
                ->setText(QStringLiteral("Alice Vance"));
            dlg->findChild<QLineEdit *>(QStringLiteral("contactNumberEdit"))
                ->setText(QStringLiteral("204"));
            dlg->findChild<QCheckBox *>(QStringLiteral("contactPresenceCheck"))->setChecked(true);
            dlg->accept();
        });

        QTest::mouseClick(window->contactsView()->addButton(), Qt::LeftButton);
        QCoreApplication::processEvents();
        QListWidget *list = window->contactsView()->listWidget();
        QCOMPARE(list->count(), 1);
        QVERIFY(list->item(0)->text().contains(QStringLiteral("Alice Vance")));

        const QRect rect = list->visualItemRect(list->item(0));
        QTest::mouseClick(list->viewport(), Qt::LeftButton, Qt::NoModifier, rect.center());
        QTest::mouseDClick(list->viewport(), Qt::LeftButton, Qt::NoModifier, rect.center());
        QCOMPARE(rig.engine.dialedCalls.size(), 1);
        QCOMPARE(rig.engine.dialedCalls.first().uri, QStringLiteral("sip:204@pbx.example.net"));
    }

    void historyRowAppearsAfterCallEndsAndMissedBadgeMarksSeenAndContactNameResolves() {
        P4UiRig rig;
        QVERIFY(rig.accounts.addAccount(sampleOfficeAccount()));

        tincan::Contact bob;
        bob.name = QStringLiteral("Bob Chen");
        bob.number = QStringLiteral("555");
        QVERIFY(rig.contacts.addContact(bob) > 0);

        auto window = rig.makeWindow();
        tincan::ui::IncomingCallPopup popup(rig.calls, &rig.contacts);
        window->show();
        QVERIFY(QTest::qWaitForWindowExposed(window.get()));

        // Incoming call from 555 without displayName -> resolved to "Bob Chen" on card & popup.
        const int callId = rig.engine.injectIncomingCall(
            QStringLiteral("acc-1"), QStringLiteral("sip:555@pbx.example.net"));
        QCoreApplication::processEvents();

        tincan::ui::CallCard *card = window->callPanel()->cardForCall(callId);
        QVERIFY(card != nullptr);
        QCOMPARE(card->callInfo().remoteDisplayName, QStringLiteral("Bob Chen"));
        QVERIFY(popup.callerText().contains(QStringLiteral("Bob Chen")));

        // Caller hangs up before answer -> missed call row + badge on History tab.
        rig.engine.injectCallState(callId, tincan::CallState::Disconnected, 487);
        QCoreApplication::processEvents();

        QCOMPARE(window->historyView()->listWidget()->count(), 1);
        QVERIFY(window->historyView()->listWidget()->item(0)->text().contains(
            QStringLiteral("Bob Chen")));
        QVERIFY(window->tabs()->tabText(2).contains(QStringLiteral("1")));

        // Opening the History tab marks missed calls as seen and clears badge.
        window->tabs()->setCurrentIndex(2);
        QCoreApplication::processEvents();
        QCOMPARE(rig.history.unseenMissedCount(), 0);
        QCOMPARE(window->tabs()->tabText(2), QStringLiteral("History"));
    }

    void chatWindowSendsMessageAndIncomingNotifiesAndOpeningMarksRead() {
        P4UiRig rig;
        QVERIFY(rig.accounts.addAccount(sampleOfficeAccount(true)));

        auto window = rig.makeWindow();
        tincan::ui::TrayIcon tray(rig.accounts, rig.calls, *window, &rig.messages, &rig.contacts);
        window->show();
        QVERIFY(QTest::qWaitForWindowExposed(window.get()));

        // Own presence selector is visible when publishPresence is true.
        QVERIFY(window->presenceCombo()->isVisible());
        window->presenceCombo()->setCurrentIndex(1); // Away
        QCOMPARE(rig.engine.ownPresenceChanges.last().state, tincan::PresenceState::Away);

        // Incoming message triggers tray notification and unread badge on Messages tab.
        rig.engine.injectIncomingMessage(tincan::MessageInfo{
            QStringLiteral("acc-1"),
            QStringLiteral("sip:204@pbx.example.net"),
            QStringLiteral("Alice"),
            QStringLiteral("Are you online?"),
            QStringLiteral("text/plain"),
            QDateTime::currentDateTimeUtc(),
        });
        QCoreApplication::processEvents();
        QCOMPARE(tray.lastNotificationBody(), QStringLiteral("Are you online?"));
        QVERIFY(window->tabs()->tabText(3).contains(QStringLiteral("1")));

        // Opening ChatWindow marks peer read and clears unread badge.
        tincan::ui::ChatWindow *chat =
            window->messagesView()->openChatForPeer(QStringLiteral("204"));
        QVERIFY(chat != nullptr);
        QVERIFY(QTest::qWaitForWindowExposed(chat));
        QCOMPARE(rig.messages.totalUnreadCount(), 0);
        QCOMPARE(window->tabs()->tabText(3), QStringLiteral("Messages"));

        // Typing a message and pressing Enter sends via FakeSipEngine.
        chat->inputEdit()->setFocus();
        QTest::keyClicks(chat->inputEdit(), QStringLiteral("Yes, calling soon!"));
        QTest::keyClick(chat->inputEdit(), Qt::Key_Return);
        QCoreApplication::processEvents();

        QCOMPARE(rig.engine.sentMessages.size(), 1);
        QCOMPARE(rig.engine.sentMessages.first().uri, QStringLiteral("sip:204@pbx.example.net"));
        QCOMPARE(rig.engine.sentMessages.first().body, QStringLiteral("Yes, calling soon!"));

        rig.engine.injectMessageStatus(rig.engine.sentMessages.first().token, 200,
                                       QStringLiteral("OK"));
        QCoreApplication::processEvents();
        QCOMPARE(chat->transcriptList()->count(), 2);
        QVERIFY(chat->transcriptList()->item(1)->text().contains(QStringLiteral("Sent")));
        chat->close();
    }
};

QTEST_MAIN(TestUiP4)
#include "tst_ui_p4.moc"
