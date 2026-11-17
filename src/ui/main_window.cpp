// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "ui/main_window.h"

#include "ui/call_panel.h"
#include "ui/dialpad_widget.h"
#include "ui/icons.h"
#include "ui/settings_dialog.h"

#include <QAction>
#include <QCloseEvent>
#include <QComboBox>
#include <QCoreApplication>
#include <QEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QSignalBlocker>
#include <QTabWidget>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

namespace tincan::ui {

namespace {

QString formatAccountShortLabel(const AccountConfig &acc) {
    if (!acc.displayName.trimmed().isEmpty()) {
        return acc.displayName.trimmed();
    }
    if (!acc.user.trimmed().isEmpty()) {
        return QStringLiteral("%1@%2").arg(acc.user.trimmed(), acc.domain.trimmed());
    }
    return acc.domain.trimmed();
}

QString formatRegistrationTooltip(const RegistrationInfo &reg, bool hasAccount) {
    if (!hasAccount) {
        return MainWindow::tr("No SIP account configured");
    }
    switch (reg.state) {
    case RegState::Unregistered:
        return MainWindow::tr("Unregistered");
    case RegState::Registering:
        return MainWindow::tr("Registering...");
    case RegState::Registered:
        return reg.statusCode > 0
                   ? MainWindow::tr("Registered (%1 %2)").arg(reg.statusCode).arg(reg.reason)
                   : MainWindow::tr("Registered");
    case RegState::Failed:
        return reg.statusCode > 0 ? MainWindow::tr("Registration failed (%1 %2)")
                                        .arg(reg.statusCode)
                                        .arg(reg.reason)
                                  : MainWindow::tr("Registration failed");
    }
    return MainWindow::tr("Unregistered");
}

bool isDtmfKeyChar(QChar ch) {
    return (ch >= u'0' && ch <= u'9') || ch == u'*' || ch == u'#';
}

} // namespace

MainWindow::MainWindow(AccountService &accountService, CallService &callService,
                       AudioService &audioService, SettingsStore &settingsStore,
                       ISecretStore &secretStore, QWidget *parent)
    : QMainWindow(parent), accountService_(accountService), callService_(callService),
      audioService_(audioService), settingsStore_(settingsStore), secretStore_(secretStore) {
    setObjectName(QStringLiteral("mainWindow"));
    setWindowTitle(tr("Tincan"));
    setWindowIcon(Icons::appIcon());
    resize(320, 560);
    setMinimumSize(290, 440);

    auto *central = new QWidget(this);
    auto *root = new QVBoxLayout(central);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    root->addWidget(buildHeaderBar());

    errorLabel_ = new QLabel(central);
    errorLabel_->setObjectName(QStringLiteral("errorBanner"));
    errorLabel_->setWordWrap(true);
    errorLabel_->setStyleSheet(QStringLiteral(
        "background-color: #fef2f2; color: #b91c1c; padding: 4px 8px; "
        "border-bottom: 1px solid #fecaca;"));
    errorLabel_->setVisible(false);
    root->addWidget(errorLabel_);

    callPanel_ = new CallPanel(callService_, central);
    root->addWidget(callPanel_);

    tabs_ = new QTabWidget(central);
    tabs_->setObjectName(QStringLiteral("mainTabs"));
    tabs_->setDocumentMode(true);

    dialpad_ = new DialpadWidget(tabs_);
    connect(dialpad_, &DialpadWidget::dialRequested, this, &MainWindow::onDialRequested);

    tabs_->addTab(dialpad_, tr("Dial"));
    tabs_->addTab(createComingSoonPlaceholder(), tr("Contacts"));
    tabs_->addTab(createComingSoonPlaceholder(), tr("History"));
    tabs_->addTab(createComingSoonPlaceholder(), tr("Messages"));
    root->addWidget(tabs_, 1);

    setCentralWidget(central);

    if (QCoreApplication *app = QCoreApplication::instance()) {
        app->installEventFilter(this);
    }

    connect(&accountService_, &AccountService::accountsChanged, this,
            &MainWindow::refreshAccountsCombo);
    connect(&accountService_, &AccountService::defaultAccountChanged, this,
            [this](const QString &) { refreshAccountsCombo(); });
    connect(&accountService_, &AccountService::registrationChanged, this,
            [this](const RegistrationInfo &) { refreshStatusIndicator(); });
    connect(&callService_, &CallService::dndChanged, this, &MainWindow::refreshDndState);
    connect(&callService_, &CallService::errorOccurred, this, &MainWindow::showErrorBanner);

    refreshAccountsCombo();
    refreshDndState(callService_.isDnd());
}

QWidget *MainWindow::buildHeaderBar() {
    auto *bar = new QWidget(this);
    bar->setObjectName(QStringLiteral("headerBar"));
    bar->setStyleSheet(QStringLiteral(
        "QWidget#headerBar { background-color: #f1f5f9; border-bottom: 1px solid #cbd5e1; }"));

    auto *layout = new QHBoxLayout(bar);
    layout->setContentsMargins(8, 6, 8, 6);
    layout->setSpacing(6);

    statusDot_ = new QLabel(bar);
    statusDot_->setObjectName(QStringLiteral("statusDot"));
    statusDot_->setFixedSize(14, 14);
    statusDot_->setAlignment(Qt::AlignCenter);

    accountCombo_ = new QComboBox(bar);
    accountCombo_->setObjectName(QStringLiteral("accountCombo"));
    connect(accountCombo_, &QComboBox::currentIndexChanged, this,
            &MainWindow::onAccountComboChanged);

    dndBadge_ = new QLabel(tr("DND"), bar);
    dndBadge_->setObjectName(QStringLiteral("dndBadge"));
    dndBadge_->setToolTip(tr("Do Not Disturb is active"));
    dndBadge_->setStyleSheet(QStringLiteral(
        "background-color: #fee2e2; color: #991b1b; border: 1px solid #fca5a5; "
        "border-radius: 3px; padding: 1px 5px; font-size: 10px; font-weight: bold;"));
    dndBadge_->setVisible(false);

    menuButton_ = new QToolButton(bar);
    menuButton_->setObjectName(QStringLiteral("menuButton"));
    menuButton_->setIcon(Icons::settings());
    menuButton_->setToolTip(tr("Menu"));
    menuButton_->setPopupMode(QToolButton::InstantPopup);
    menuButton_->setMenu(buildHeaderMenu(menuButton_));

    layout->addWidget(statusDot_);
    layout->addWidget(accountCombo_, 1);
    layout->addWidget(dndBadge_);
    layout->addWidget(menuButton_);
    return bar;
}

QMenu *MainWindow::buildHeaderMenu(QWidget *parent) {
    auto *menu = new QMenu(parent);
    dndAction_ = menu->addAction(tr("Do Not Disturb"));
    dndAction_->setCheckable(true);
    dndAction_->setChecked(callService_.isDnd());
    connect(dndAction_, &QAction::toggled, &callService_, &CallService::setDnd);

    menu->addSeparator();
    QAction *settingsAct = menu->addAction(Icons::settings(), tr("Settings..."));
    settingsAct->setShortcut(QKeySequence::Preferences);
    connect(settingsAct, &QAction::triggered, this, &MainWindow::openSettingsDialog);
    addAction(settingsAct);

    menu->addSeparator();
    QAction *quitAct = menu->addAction(tr("Quit"));
    quitAct->setShortcut(QKeySequence::Quit);
    connect(quitAct, &QAction::triggered, this, &MainWindow::requestQuit);
    addAction(quitAct);
    return menu;
}

QWidget *MainWindow::createComingSoonPlaceholder() const {
    auto *page = new QWidget();
    auto *vbox = new QVBoxLayout(page);
    auto *label = new QLabel(tr("Coming soon"), page);
    label->setAlignment(Qt::AlignCenter);
    label->setStyleSheet(QStringLiteral("color: #64748b; font-size: 13px;"));
    vbox->addWidget(label);
    return page;
}

DialpadWidget *MainWindow::dialpad() const { return dialpad_; }
CallPanel *MainWindow::callPanel() const { return callPanel_; }
QLabel *MainWindow::dndBadge() const { return dndBadge_; }
QAction *MainWindow::dndAction() const { return dndAction_; }

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
        if (widget != nullptr && widget->window() == this) {
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

void MainWindow::refreshAccountsCombo() {
    updatingCombo_ = true;
    accountCombo_->clear();

    const QList<AccountConfig> list = accountService_.accounts();
    const QString defId = accountService_.defaultAccountId();
    if (list.isEmpty()) {
        accountCombo_->addItem(tr("No account configured"), QString{});
        accountCombo_->setEnabled(false);
    } else {
        accountCombo_->setEnabled(true);
        int selectedIdx = 0;
        for (qsizetype i = 0; i < list.size(); ++i) {
            accountCombo_->addItem(formatAccountShortLabel(list[i]), list[i].id);
            if (list[i].id == defId) {
                selectedIdx = static_cast<int>(i);
            }
        }
        accountCombo_->setCurrentIndex(selectedIdx);
    }
    updatingCombo_ = false;
    refreshStatusIndicator();
}

void MainWindow::refreshStatusIndicator() {
    const QString defId = accountService_.defaultAccountId();
    const bool hasAcc = !defId.isEmpty();
    const RegistrationInfo reg =
        hasAcc ? accountService_.registrationInfo(defId) : RegistrationInfo{};

    statusDot_->setPixmap(Icons::statusDotPixmap(reg.state, 12));
    const QString tip = formatRegistrationTooltip(reg, hasAcc);
    statusDot_->setToolTip(tip);
    accountCombo_->setToolTip(tip);
}

void MainWindow::refreshDndState(bool enabled) {
    dndBadge_->setVisible(enabled);
    const QSignalBlocker blocker(dndAction_);
    dndAction_->setChecked(enabled);
}

void MainWindow::onAccountComboChanged(int index) {
    if (updatingCombo_ || index < 0) {
        return;
    }
    const QString id = accountCombo_->itemData(index).toString();
    if (!id.isEmpty()) {
        accountService_.setDefaultAccountId(id);
    }
}

void MainWindow::onDialRequested(const QString &text) {
    errorLabel_->setVisible(false);
    const QString err = callService_.dial(text);
    if (err.isEmpty()) {
        dialpad_->clearText();
    }
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
