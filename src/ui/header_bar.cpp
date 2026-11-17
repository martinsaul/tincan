// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "ui/header_bar.h"

#include "ui/icons.h"

#include <QAction>
#include <QComboBox>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QSignalBlocker>
#include <QToolButton>

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
        return HeaderBar::tr("No SIP account configured");
    }
    switch (reg.state) {
    case RegState::Unregistered:
        return HeaderBar::tr("Unregistered");
    case RegState::Registering:
        return HeaderBar::tr("Registering...");
    case RegState::Registered:
        return reg.statusCode > 0
                   ? HeaderBar::tr("Registered (%1 %2)").arg(reg.statusCode).arg(reg.reason)
                   : HeaderBar::tr("Registered");
    case RegState::Failed:
        return reg.statusCode > 0 ? HeaderBar::tr("Registration failed (%1 %2)")
                                        .arg(reg.statusCode)
                                        .arg(reg.reason)
                                  : HeaderBar::tr("Registration failed");
    }
    return HeaderBar::tr("Unregistered");
}

} // namespace

HeaderBar::HeaderBar(AccountService &accountService, CallService &callService, QWidget *parent)
    : QWidget(parent), accountService_(accountService), callService_(callService) {
    setupWidgets();

    connect(&accountService_, &AccountService::accountsChanged, this,
            &HeaderBar::refreshAccountsCombo);
    connect(&accountService_, &AccountService::defaultAccountChanged, this,
            [this](const QString &) { refreshAccountsCombo(); });
    connect(&accountService_, &AccountService::registrationChanged, this,
            [this](const RegistrationInfo &) { refreshStatusIndicator(); });
    connect(&accountService_, &AccountService::ownPresenceChanged, this,
            [this](PresenceState) { refreshPresenceSelector(); });
    connect(&callService_, &CallService::dndChanged, this, &HeaderBar::refreshDndState);

    refreshAccountsCombo();
    refreshDndState(callService_.isDnd());
}

void HeaderBar::setupWidgets() {
    setObjectName(QStringLiteral("headerBar"));
    setStyleSheet(QStringLiteral(
        "QWidget#headerBar { background-color: #f1f5f9; border-bottom: 1px solid #cbd5e1; }"));

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 6, 8, 6);
    layout->setSpacing(6);

    statusDot_ = new QLabel(this);
    statusDot_->setObjectName(QStringLiteral("statusDot"));
    statusDot_->setFixedSize(14, 14);
    statusDot_->setAlignment(Qt::AlignCenter);

    accountCombo_ = new QComboBox(this);
    accountCombo_->setObjectName(QStringLiteral("accountCombo"));
    connect(accountCombo_, &QComboBox::currentIndexChanged, this,
            &HeaderBar::onAccountComboChanged);

    presenceCombo_ = new QComboBox(this);
    presenceCombo_->setObjectName(QStringLiteral("presenceCombo"));
    presenceCombo_->setToolTip(tr("Own presence"));
    presenceCombo_->addItem(Icons::presenceDotIcon(PresenceState::Online), tr("Online"),
                            static_cast<int>(PresenceState::Online));
    presenceCombo_->addItem(Icons::presenceDotIcon(PresenceState::Away), tr("Away"),
                            static_cast<int>(PresenceState::Away));
    presenceCombo_->addItem(Icons::presenceDotIcon(PresenceState::Busy), tr("Busy"),
                            static_cast<int>(PresenceState::Busy));
    presenceCombo_->setVisible(false);
    connect(presenceCombo_, &QComboBox::currentIndexChanged, this,
            &HeaderBar::onPresenceComboChanged);

    dndBadge_ = new QLabel(tr("DND"), this);
    dndBadge_->setObjectName(QStringLiteral("dndBadge"));
    dndBadge_->setToolTip(tr("Do Not Disturb is active"));
    dndBadge_->setStyleSheet(QStringLiteral(
        "background-color: #fee2e2; color: #991b1b; border: 1px solid #fca5a5; "
        "border-radius: 3px; padding: 1px 5px; font-size: 10px; font-weight: bold;"));
    dndBadge_->setVisible(false);

    menuButton_ = new QToolButton(this);
    menuButton_->setObjectName(QStringLiteral("menuButton"));
    menuButton_->setIcon(Icons::settings());
    menuButton_->setToolTip(tr("Menu"));
    menuButton_->setPopupMode(QToolButton::InstantPopup);
    menuButton_->setMenu(buildMenu(menuButton_));

    layout->addWidget(statusDot_);
    layout->addWidget(accountCombo_, 1);
    layout->addWidget(presenceCombo_);
    layout->addWidget(dndBadge_);
    layout->addWidget(menuButton_);
}

QMenu *HeaderBar::buildMenu(QWidget *parent) {
    auto *menu = new QMenu(parent);
    dndAction_ = menu->addAction(tr("Do Not Disturb"));
    dndAction_->setCheckable(true);
    dndAction_->setChecked(callService_.isDnd());
    connect(dndAction_, &QAction::toggled, &callService_, &CallService::setDnd);

    menu->addSeparator();
    settingsAction_ = menu->addAction(Icons::settings(), tr("Settings..."));
    settingsAction_->setShortcut(QKeySequence::Preferences);
    connect(settingsAction_, &QAction::triggered, this, &HeaderBar::settingsRequested);
    aboutAction_ = menu->addAction(tr("About Tincan"));
    aboutAction_->setObjectName(QStringLiteral("aboutAction"));
    connect(aboutAction_, &QAction::triggered, this, &HeaderBar::aboutRequested);

    menu->addSeparator();
    quitAction_ = menu->addAction(tr("Quit"));
    quitAction_->setShortcut(QKeySequence::Quit);
    connect(quitAction_, &QAction::triggered, this, &HeaderBar::quitRequested);
    return menu;
}

QComboBox *HeaderBar::presenceCombo() const { return presenceCombo_; }
QLabel *HeaderBar::dndBadge() const { return dndBadge_; }
QAction *HeaderBar::dndAction() const { return dndAction_; }
QAction *HeaderBar::settingsAction() const { return settingsAction_; }
QAction *HeaderBar::aboutAction() const { return aboutAction_; }
QAction *HeaderBar::quitAction() const { return quitAction_; }

void HeaderBar::refreshAccountsCombo() {
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
    refreshPresenceSelector();
}

void HeaderBar::refreshPresenceSelector() {
    const auto defAcc = accountService_.defaultAccount();
    presenceCombo_->setVisible(defAcc.has_value() && defAcc->publishPresence);

    const QSignalBlocker blocker(presenceCombo_);
    const int idx = presenceCombo_->findData(static_cast<int>(accountService_.ownPresence()));
    if (idx >= 0) {
        presenceCombo_->setCurrentIndex(idx);
    }
}

void HeaderBar::refreshStatusIndicator() {
    const QString defId = accountService_.defaultAccountId();
    const bool hasAcc = !defId.isEmpty();
    const RegistrationInfo reg =
        hasAcc ? accountService_.registrationInfo(defId) : RegistrationInfo{};

    statusDot_->setPixmap(Icons::statusDotPixmap(reg.state, 12));
    const QString tip = formatRegistrationTooltip(reg, hasAcc);
    statusDot_->setToolTip(tip);
    accountCombo_->setToolTip(tip);
}

void HeaderBar::refreshDndState(bool enabled) {
    dndBadge_->setVisible(enabled);
    const QSignalBlocker blocker(dndAction_);
    dndAction_->setChecked(enabled);
}

void HeaderBar::onAccountComboChanged(int index) {
    if (updatingCombo_ || index < 0) {
        return;
    }
    const QString id = accountCombo_->itemData(index).toString();
    if (!id.isEmpty()) {
        accountService_.setDefaultAccountId(id);
    }
}

void HeaderBar::onPresenceComboChanged(int index) {
    if (index < 0) {
        return;
    }
    const auto state = static_cast<PresenceState>(presenceCombo_->itemData(index).toInt());
    (void)accountService_.setOwnPresence(state);
}

} // namespace tincan::ui
