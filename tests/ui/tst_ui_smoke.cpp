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
#include "ui/account_dialog.h"
#include "ui/accounts_page.h"
#include "ui/call_card.h"
#include "ui/call_panel.h"
#include "ui/dialpad_widget.h"
#include "ui/dtmf_pad.h"
#include "ui/incoming_call_popup.h"
#include "ui/main_window.h"
#include "ui/settings_dialog.h"
#include "ui/transfer_dialog.h"
#include "ui/tray_icon.h"

#include <QAction>
#include <QApplication>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSlider>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

namespace {

tincan::AccountConfig sampleOfficeAccount() {
    tincan::AccountConfig acc;
    acc.id = QStringLiteral("acc-1");
    acc.displayName = QStringLiteral("Main Office");
    acc.user = QStringLiteral("100");
    acc.domain = QStringLiteral("pbx.example.net");
    return acc;
}

tincan::Database &openMemoryDb(tincan::Database &db) {
    (void)db.open();
    return db;
}

struct SmokeRig {
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

    SmokeRig() {
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

class TestUiSmoke : public QObject {
    Q_OBJECT

private slots:
    void typingDigitsAndClickingCallDialsNormalizedUri() {
        SmokeRig rig;
        QVERIFY(rig.tempDir.isValid());
        QVERIFY(rig.accounts.addAccount(sampleOfficeAccount()));

        auto window = rig.makeWindow();
        window->show();
        QVERIFY(QTest::qWaitForWindowExposed(window.get()));

        QLineEdit *edit = window->dialpad()->numberEdit();
        edit->setFocus();
        QTest::keyClicks(edit, QStringLiteral("123"));
        QCOMPARE(edit->text(), QStringLiteral("123"));

        QTest::mouseClick(window->dialpad()->callButton(), Qt::LeftButton);
        QCOMPARE(rig.engine.dialedCalls.size(), 1);
        QCOMPARE(rig.engine.dialedCalls.first().accountId, QStringLiteral("acc-1"));
        QCOMPARE(rig.engine.dialedCalls.first().uri, QStringLiteral("sip:123@pbx.example.net"));
    }

    void incomingCallShowsCallCardWithAnswerAndAnswersOnEngine() {
        SmokeRig rig;
        QVERIFY(rig.tempDir.isValid());
        QVERIFY(rig.accounts.addAccount(sampleOfficeAccount()));

        auto window = rig.makeWindow();
        tincan::ui::IncomingCallPopup popup(rig.calls, &rig.contacts);
        window->show();
        QVERIFY(QTest::qWaitForWindowExposed(window.get()));
        QVERIFY(!window->callPanel()->isVisible());

        const int callId = rig.engine.injectIncomingCall(
            QStringLiteral("acc-1"), QStringLiteral("sip:555@pbx.example.net"),
            QStringLiteral("Caller Five"));
        QCoreApplication::processEvents();

        QVERIFY(window->callPanel()->isVisible());
        QVERIFY(popup.isVisible());
        tincan::ui::CallCard *card = window->callPanel()->cardForCall(callId);
        QVERIFY(card != nullptr);
        QVERIFY(card->answerButton()->isVisible());

        QTest::mouseClick(card->answerButton(), Qt::LeftButton);
        QCOMPARE(rig.engine.answeredCallIds, QList<int>{callId});

        rig.engine.injectCallState(callId, tincan::CallState::Confirmed, 200);
        QCoreApplication::processEvents();
        QVERIFY(!card->answerButton()->isVisible());
        QVERIFY(!popup.isVisible());

        rig.engine.injectCallState(callId, tincan::CallState::Disconnected, 200);
        QCoreApplication::processEvents();
        QVERIFY(!window->callPanel()->isVisible());
    }

    void settingsDialogAddsAccountViaAccountDialogIntoSettingsStore() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        tincan::SettingsStore settings(tempDir.filePath(QStringLiteral("tincan.ini")));
        tincan::MemorySecretStore secrets;
        tincan::test::FakeSipEngine engine;
        tincan::AccountService accounts(engine, settings, secrets);
        tincan::AudioService audio(engine, settings);

        tincan::ui::SettingsDialog dlg(accounts, audio, settings, secrets);
        dlg.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dlg));

        auto *addBtn =
            dlg.accountsPage()->findChild<QPushButton *>(QStringLiteral("addAccountButton"));
        QVERIFY(addBtn != nullptr);

        QTimer::singleShot(0, &dlg, []() {
            auto *modal =
                qobject_cast<tincan::ui::AccountDialog *>(QApplication::activeModalWidget());
            QVERIFY(modal != nullptr);

            modal->findChild<QLineEdit *>(QStringLiteral("displayNameEdit"))
                ->setText(QStringLiteral("Engineering Line"));
            modal->findChild<QLineEdit *>(QStringLiteral("userEdit"))
                ->setText(QStringLiteral("777"));
            modal->findChild<QLineEdit *>(QStringLiteral("domainEdit"))
                ->setText(QStringLiteral("sip.engineering.example"));
            modal->findChild<QLineEdit *>(QStringLiteral("passwordEdit"))
                ->setText(QStringLiteral("VaultPass#42"));
            modal->accept();
        });

        QTest::mouseClick(addBtn, Qt::LeftButton);
        dlg.accept();

        const QList<tincan::AccountConfig> stored = settings.accounts();
        QCOMPARE(stored.size(), 1);
        QCOMPARE(stored.first().displayName, QStringLiteral("Engineering Line"));
        QCOMPARE(stored.first().user, QStringLiteral("777"));
        QCOMPARE(stored.first().domain, QStringLiteral("sip.engineering.example"));
        QVERIFY(stored.first().password.isEmpty());
        QCOMPARE(secrets.peekPassword(stored.first().id), QStringLiteral("VaultPass#42"));
    }

    void inCallControlsKeyboardDtmfAndBlindTransferWork() {
        SmokeRig rig;
        QVERIFY(rig.tempDir.isValid());
        QVERIFY(rig.accounts.addAccount(sampleOfficeAccount()));

        auto window = rig.makeWindow();
        window->show();
        QVERIFY(QTest::qWaitForWindowExposed(window.get()));

        QVERIFY(rig.calls.dial(QStringLiteral("201")).isEmpty());
        const int callId = rig.engine.dialedCalls.first().callId;
        rig.engine.injectCallState(callId, tincan::CallState::Confirmed, 200);
        QCoreApplication::processEvents();

        tincan::ui::CallCard *card = window->callPanel()->cardForCall(callId);
        QVERIFY(card != nullptr);
        QTest::mouseClick(card->holdButton(), Qt::LeftButton);
        QVERIFY(card->callInfo().localHold);
        QTest::mouseClick(card->holdButton(), Qt::LeftButton);
        QVERIFY(!card->callInfo().localHold);

        QTest::mouseClick(card->muteButton(), Qt::LeftButton);
        QVERIFY(card->callInfo().muted);
        QTest::mouseClick(card->recordButton(), Qt::LeftButton);
        QVERIFY(card->callInfo().recording);
        QVERIFY(card->recordButton()->styleSheet().contains(QStringLiteral("#dc2626")));
        card->volumeSlider()->setValue(150);
        QCOMPARE(rig.engine.volumeChanges.last().rxLevel, 1.5f);

        // Keyboard digits while connected send DTMF instead of filling the dial field.
        window->dialpad()->numberEdit()->setFocus();
        QTest::keyClicks(window->dialpad()->numberEdit(), QStringLiteral("42#"));
        QVERIFY(window->dialpad()->text().isEmpty());
        QCOMPARE(card->dtmfEcho(), QStringLiteral("42#"));

        QTimer::singleShot(0, window.get(), []() {
            auto *dlg =
                qobject_cast<tincan::ui::TransferDialog *>(QApplication::activeModalWidget());
            QVERIFY(dlg != nullptr);
            dlg->findChild<QLineEdit *>(QStringLiteral("transferDestinationEdit"))
                ->setText(QStringLiteral("309"));
            dlg->accept();
        });
        QTest::mouseClick(card->transferButton(), Qt::LeftButton);
        QCOMPARE(rig.engine.blindTransfers.size(), 1);
        QCOMPARE(rig.engine.blindTransfers.first().uri, QStringLiteral("sip:309@pbx.example.net"));
    }

    void multiCallSwitchMergeAndDndBadgeWork() {
        SmokeRig rig;
        QVERIFY(rig.tempDir.isValid());
        QVERIFY(rig.accounts.addAccount(sampleOfficeAccount()));

        auto window = rig.makeWindow();
        tincan::ui::TrayIcon tray(rig.accounts, rig.calls, *window, &rig.messages, &rig.contacts);
        window->show();
        QVERIFY(QTest::qWaitForWindowExposed(window.get()));

        QVERIFY(!window->dndBadge()->isVisible());
        tray.dndAction()->trigger();
        QVERIFY(window->dndBadge()->isVisible());
        window->dndAction()->trigger();
        QVERIFY(!window->dndBadge()->isVisible());

        QVERIFY(rig.calls.dial(QStringLiteral("201")).isEmpty());
        const int firstId = rig.engine.dialedCalls.first().callId;
        rig.engine.injectCallState(firstId, tincan::CallState::Confirmed, 200);
        QVERIFY(rig.calls.dial(QStringLiteral("202")).isEmpty());
        const int secondId = rig.engine.dialedCalls.last().callId;
        rig.engine.injectCallState(secondId, tincan::CallState::Confirmed, 200);
        QCoreApplication::processEvents();

        QVERIFY(window->callPanel()->mergeButton()->isVisible());
        tincan::ui::CallCard *card1 = window->callPanel()->cardForCall(firstId);
        tincan::ui::CallCard *card2 = window->callPanel()->cardForCall(secondId);
        QVERIFY(card1->callInfo().localHold);
        QVERIFY(!card2->callInfo().localHold);

        QTest::mouseClick(card1, Qt::LeftButton);
        QVERIFY(!card1->callInfo().localHold);
        QVERIFY(card2->callInfo().localHold);

        QTest::mouseClick(window->callPanel()->mergeButton(), Qt::LeftButton);
        QCOMPARE(rig.engine.conferenceCalls.size(), 1);
    }
};

QTEST_MAIN(TestUiSmoke)
#include "tst_ui_smoke.moc"
