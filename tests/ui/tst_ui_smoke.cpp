// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "app/account_service.h"
#include "app/audio_service.h"
#include "app/call_service.h"
#include "app/secret_store.h"
#include "app/settings_store.h"
#include "fake_sip_engine.h"
#include "ui/account_dialog.h"
#include "ui/accounts_page.h"
#include "ui/call_card.h"
#include "ui/call_panel.h"
#include "ui/dialpad_widget.h"
#include "ui/incoming_call_popup.h"
#include "ui/main_window.h"
#include "ui/settings_dialog.h"

#include <QApplication>
#include <QLineEdit>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

class TestUiSmoke : public QObject {
    Q_OBJECT

private slots:
    void typingDigitsAndClickingCallDialsNormalizedUri() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        tincan::SettingsStore settings(tempDir.filePath(QStringLiteral("tincan.ini")));
        tincan::MemorySecretStore secrets;
        tincan::test::FakeSipEngine engine;
        tincan::AccountService accounts(engine, settings, secrets);
        tincan::CallService calls(engine, accounts, settings);
        tincan::AudioService audio(engine, settings);
        engine.setEvents(&calls);

        tincan::AccountConfig acc;
        acc.id = QStringLiteral("acc-1");
        acc.displayName = QStringLiteral("Main Office");
        acc.user = QStringLiteral("100");
        acc.domain = QStringLiteral("pbx.example.net");
        QVERIFY(accounts.addAccount(acc));

        tincan::ui::MainWindow window(accounts, calls, audio, settings, secrets);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));

        QLineEdit *edit = window.dialpad()->numberEdit();
        QVERIFY(edit != nullptr);
        edit->setFocus();
        QTest::keyClicks(edit, QStringLiteral("123"));
        QCOMPARE(edit->text(), QStringLiteral("123"));

        QTest::mouseClick(window.dialpad()->callButton(), Qt::LeftButton);
        QCOMPARE(engine.dialedCalls.size(), 1);
        QCOMPARE(engine.dialedCalls.first().accountId, QStringLiteral("acc-1"));
        QCOMPARE(engine.dialedCalls.first().uri, QStringLiteral("sip:123@pbx.example.net"));
    }

    void incomingCallShowsCallCardWithAnswerAndAnswersOnEngine() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        tincan::SettingsStore settings(tempDir.filePath(QStringLiteral("tincan.ini")));
        tincan::MemorySecretStore secrets;
        tincan::test::FakeSipEngine engine;
        tincan::AccountService accounts(engine, settings, secrets);
        tincan::CallService calls(engine, accounts, settings);
        tincan::AudioService audio(engine, settings);
        engine.setEvents(&calls);

        tincan::AccountConfig acc;
        acc.id = QStringLiteral("acc-1");
        acc.user = QStringLiteral("100");
        acc.domain = QStringLiteral("pbx.example.net");
        QVERIFY(accounts.addAccount(acc));

        tincan::ui::MainWindow window(accounts, calls, audio, settings, secrets);
        tincan::ui::IncomingCallPopup popup(calls);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QVERIFY(!window.callPanel()->isVisible());

        const int callId = engine.injectIncomingCall(
            QStringLiteral("acc-1"), QStringLiteral("sip:555@pbx.example.net"),
            QStringLiteral("Caller Five"));
        QCoreApplication::processEvents();

        QVERIFY(window.callPanel()->isVisible());
        QVERIFY(popup.isVisible());
        tincan::ui::CallCard *card = window.callPanel()->cardForCall(callId);
        QVERIFY(card != nullptr);
        QVERIFY(card->answerButton()->isVisible());

        QTest::mouseClick(card->answerButton(), Qt::LeftButton);
        QCOMPARE(engine.answeredCallIds, QList<int>{callId});

        engine.injectCallState(callId, tincan::CallState::Confirmed, 200);
        QCoreApplication::processEvents();
        QVERIFY(!card->answerButton()->isVisible());
        QVERIFY(!popup.isVisible());

        engine.injectCallState(callId, tincan::CallState::Disconnected, 200);
        QCoreApplication::processEvents();
        QVERIFY(!window.callPanel()->isVisible());
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
            auto *modal = qobject_cast<tincan::ui::AccountDialog *>(
                QApplication::activeModalWidget());
            QVERIFY(modal != nullptr);

            auto *displayEdit = modal->findChild<QLineEdit *>(QStringLiteral("displayNameEdit"));
            auto *userEdit = modal->findChild<QLineEdit *>(QStringLiteral("userEdit"));
            auto *domainEdit = modal->findChild<QLineEdit *>(QStringLiteral("domainEdit"));
            auto *passwordEdit = modal->findChild<QLineEdit *>(QStringLiteral("passwordEdit"));
            QVERIFY(displayEdit != nullptr);
            QVERIFY(userEdit != nullptr);
            QVERIFY(domainEdit != nullptr);
            QVERIFY(passwordEdit != nullptr);

            displayEdit->setText(QStringLiteral("Engineering Line"));
            userEdit->setText(QStringLiteral("777"));
            domainEdit->setText(QStringLiteral("sip.engineering.example"));
            passwordEdit->setText(QStringLiteral("VaultPass#42"));
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
};

QTEST_MAIN(TestUiSmoke)
#include "tst_ui_smoke.moc"
