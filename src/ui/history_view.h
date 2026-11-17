// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include "app/account_service.h"
#include "app/contact_service.h"
#include "app/history_service.h"

#include <QPoint>
#include <QWidget>

class QComboBox;
class QLineEdit;
class QListWidget;
class QListWidgetItem;

namespace tincan::ui {

class HistoryView : public QWidget {
    Q_OBJECT

public:
    HistoryView(HistoryService &historyService, ContactService &contactService,
                AccountService &accountService, QWidget *parent = nullptr);

    QComboBox *filterCombo() const;
    QLineEdit *searchEdit() const;
    QListWidget *listWidget() const;

signals:
    void callRequested(const QString &uri, const QString &accountId);
    void messageRequested(const QString &uri, const QString &accountId);

private:
    QWidget *buildToolbar();
    void refreshList();
    void onItemDoubleClicked(QListWidgetItem *item);
    void onContextMenuRequested(const QPoint &pos);
    void addEntryToContacts(const QString &uri, const QString &name, const QString &accountId);

    HistoryService &historyService_;
    ContactService &contactService_;
    AccountService &accountService_;
    QComboBox *filterCombo_ = nullptr;
    QLineEdit *searchEdit_ = nullptr;
    QListWidget *listWidget_ = nullptr;
};

} // namespace tincan::ui
