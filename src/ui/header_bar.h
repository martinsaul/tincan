// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include "app/account_service.h"
#include "app/call_service.h"

#include <QWidget>

class QAction;
class QComboBox;
class QLabel;
class QMenu;
class QToolButton;

namespace tincan::ui {

class HeaderBar : public QWidget {
    Q_OBJECT

public:
    HeaderBar(AccountService &accountService, CallService &callService, QWidget *parent = nullptr);

    QComboBox *presenceCombo() const;
    QLabel *dndBadge() const;
    QAction *dndAction() const;
    QAction *settingsAction() const;
    QAction *aboutAction() const;
    QAction *quitAction() const;

signals:
    void settingsRequested();
    void aboutRequested();
    void quitRequested();

private:
    void setupWidgets();
    QMenu *buildMenu(QWidget *parent);
    void refreshAccountsCombo();
    void refreshPresenceSelector();
    void refreshStatusIndicator();
    void refreshDndState(bool enabled);
    void onAccountComboChanged(int index);
    void onPresenceComboChanged(int index);

    AccountService &accountService_;
    CallService &callService_;
    bool updatingCombo_ = false;

    QLabel *statusDot_ = nullptr;
    QComboBox *accountCombo_ = nullptr;
    QComboBox *presenceCombo_ = nullptr;
    QLabel *dndBadge_ = nullptr;
    QAction *dndAction_ = nullptr;
    QAction *settingsAction_ = nullptr;
    QAction *aboutAction_ = nullptr;
    QAction *quitAction_ = nullptr;
    QToolButton *menuButton_ = nullptr;
};

} // namespace tincan::ui
