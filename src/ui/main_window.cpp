// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "ui/main_window.h"

#include "ui/call_panel.h"
#include "ui/contacts_view.h"
#include "ui/dialpad_widget.h"
#include "ui/header_bar.h"
#include "ui/history_view.h"
#include "ui/icons.h"
#include "ui/messages_view.h"
#include "ui/settings_dialog.h"

#include <QAction>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>

namespace tincan::ui {

namespace {

bool isDtmfKeyChar(QChar ch) {
    return (ch >= u'0' && ch <= u'9') || ch == u'*' || ch == u'#';
}

} // namespace

MainWindow::MainWindow(AccountService &accountService, CallService &callService,
                       AudioService &audioService, ContactService &contactService,
                       HistoryService &historyService, MessageService &messageService,
                       SettingsStore &settingsStore, ISecretStore &secretStore, QWidget *parent)
    : QMainWindow(parent), accountService_(accountService), callService_(callService),
      audioService_(audioService), contactService_(contactService), historyService_(historyService),
      messageService_(messageService), settingsStore_(settingsStore), secretStore_(secretStore) {
    setObjectName(QStringLiteral("mainWindow"));
    setWindowTitle(tr("Tincan"));
    setWindowIcon(Icons::appIcon());
    resize(320, 560);
    setMinimumSize(290, 440);

    auto *central = new QWidget(this);
    auto *root = new QVBoxLayout(central);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    headerBar_ = new HeaderBar(accountService_, callService_, central);
    addAction(headerBar_->settingsAction());
    addAction(headerBar_->quitAction());
    root->addWidget(headerBar_);

    errorLabel_ = new QLabel(central);
    errorLabel_->setObjectName(QStringLiteral("errorBanner"));
    errorLabel_->setWordWrap(true);
    errorLabel_->setStyleSheet(QStringLiteral(
        "background-color: #fef2f2; color: #b91c1c; padding: 4px 8px; "
        "border-bottom: 1px solid #fecaca;"));
    errorLabel_->setVisible(false);
    root->addWidget(errorLabel_);

    callPanel_ = new CallPanel(callService_, &contactService_, central);
    root->addWidget(callPanel_);

    setupTabs(central);
    root->addWidget(tabs_, 1);
    setCentralWidget(central);

    if (QCoreApplication *app = QCoreApplication::instance()) {
        app->installEventFilter(this);
    }

    connectSignals();
    refreshHistoryTabBadge();
    refreshMessagesTabBadge();
}

void MainWindow::setupTabs(QWidget *central) {
    tabs_ = new QTabWidget(central);
    tabs_->setObjectName(QStringLiteral("mainTabs"));
    tabs_->setDocumentMode(true);

    dialpad_ = new DialpadWidget(tabs_);
    contactsView_ = new ContactsView(contactService_, accountService_, tabs_);
    historyView_ = new HistoryView(historyService_, contactService_, accountService_, tabs_);
    messagesView_ = new MessagesView(messageService_, contactService_, accountService_, tabs_);

    tabs_->addTab(dialpad_, tr("Dial"));
    tabs_->addTab(contactsView_, tr("Contacts"));
    historyTabIndex_ = tabs_->addTab(historyView_, tr("History"));
    messagesTabIndex_ = tabs_->addTab(messagesView_, tr("Messages"));
}

void MainWindow::connectSignals() {
    connect(headerBar_, &HeaderBar::settingsRequested, this, &MainWindow::openSettingsDialog);
    connect(headerBar_, &HeaderBar::quitRequested, this, &MainWindow::requestQuit);
    connect(dialpad_, &DialpadWidget::dialRequested, this, &MainWindow::onDialRequested);
    connect(contactsView_, &ContactsView::callRequested, this,
            &MainWindow::onDialOnAccountRequested);
    connect(contactsView_, &ContactsView::messageRequested, this,
            [this](const QString &num, const QString &accId) {
                (void)messagesView_->openChatForPeer(num, accId);
            });
    connect(historyView_, &HistoryView::callRequested, this,
            &MainWindow::onDialOnAccountRequested);
    connect(historyView_, &HistoryView::messageRequested, this,
            [this](const QString &uri, const QString &accId) {
                (void)messagesView_->openChatForPeer(uri, accId);
            });
    connect(messagesView_, &MessagesView::callRequested, this,
            &MainWindow::onDialOnAccountRequested);
    connect(tabs_, &QTabWidget::currentChanged, this, &MainWindow::onTabChanged);

    connect(&historyService_, &HistoryService::missedCountChanged, this,
            [this](int) { refreshHistoryTabBadge(); });
    connect(&messageService_, &MessageService::unreadCountChanged, this,
            [this](int) { refreshMessagesTabBadge(); });
    connect(&callService_, &CallService::errorOccurred, this, &MainWindow::showErrorBanner);
}

DialpadWidget *MainWindow::dialpad() const { return dialpad_; }
CallPanel *MainWindow::callPanel() const { return callPanel_; }
ContactsView *MainWindow::contactsView() const { return contactsView_; }
HistoryView *MainWindow::historyView() const { return historyView_; }
MessagesView *MainWindow::messagesView() const { return messagesView_; }
QTabWidget *MainWindow::tabs() const { return tabs_; }
QComboBox *MainWindow::presenceCombo() const { return headerBar_->presenceCombo(); }
QLabel *MainWindow::dndBadge() const { return headerBar_->dndBadge(); }
QAction *MainWindow::dndAction() const { return headerBar_->dndAction(); }

void MainWindow::openSettingsDialog() {
    SettingsDialog dlg(accountService_, audioService_, settingsStore_, secretStore_, this);
    if (dlg.exec() == QDialog::Accepted) {
        callService_.reloadSettings();
    }
}

void MainWindow::requestQuit() {
    quitting_ = true;
    QCoreApplication::quit();
}

void MainWindow::closeEvent(QCloseEvent *event) {
    if (!quitting_ && settingsStore_.generalSettings().closeToTray) {
        hide();
        event->ignore();
        return;
    }
    QMainWindow::closeEvent(event);
    emit closedWithoutTray();
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event) {
    if (event->type() == QEvent::KeyPress) {
        auto *widget = qobject_cast<QWidget *>(watched);
        auto *lineEdit = qobject_cast<QLineEdit *>(watched);
        const bool isOtherLineEdit = (lineEdit != nullptr && lineEdit != dialpad_->numberEdit());
        if (!isOtherLineEdit && widget != nullptr && widget->window() == this) {
            const int activeId = callService_.activeConfirmedCallId();
            auto *keyEvent = static_cast<QKeyEvent *>(event);
            const Qt::KeyboardModifiers disallowed =
                Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier;
            const QString typed = keyEvent->text();
            if (activeId >= 0 && (keyEvent->modifiers() & disallowed) == 0 && typed.size() == 1 &&
                isDtmfKeyChar(typed.front())) {
                (void)callService_.sendDtmf(activeId, typed);
                return true;
            }
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::refreshHistoryTabBadge() {
    if (historyTabIndex_ < 0) {
        return;
    }
    if (tabs_->currentIndex() == historyTabIndex_ && isVisible() &&
        historyService_.unseenMissedCount() > 0) {
        historyService_.markAllSeen();
        return;
    }
    const int missed = historyService_.unseenMissedCount();
    tabs_->setTabText(historyTabIndex_,
                      missed > 0 ? tr("History (%1)").arg(missed) : tr("History"));
}

void MainWindow::refreshMessagesTabBadge() {
    if (messagesTabIndex_ < 0) {
        return;
    }
    const int unread = messageService_.totalUnreadCount();
    tabs_->setTabText(messagesTabIndex_,
                      unread > 0 ? tr("Messages (%1)").arg(unread) : tr("Messages"));
}

void MainWindow::onTabChanged(int index) {
    if (index == historyTabIndex_) {
        historyService_.markAllSeen();
    }
}

void MainWindow::onDialRequested(const QString &text) {
    errorLabel_->setVisible(false);
    const QString err = callService_.dial(text);
    if (err.isEmpty()) {
        dialpad_->clearText();
    }
}

void MainWindow::onDialOnAccountRequested(const QString &target, const QString &accountId) {
    errorLabel_->setVisible(false);
    (void)callService_.dialOnAccount(accountId, target);
}

void MainWindow::showErrorBanner(const QString &message) {
    errorLabel_->setText(message);
    errorLabel_->setVisible(!message.isEmpty());
    QTimer::singleShot(5000, this, [this, message]() {
        if (errorLabel_->text() == message) {
            errorLabel_->setVisible(false);
        }
    });
}

} // namespace tincan::ui
