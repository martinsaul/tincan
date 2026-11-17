// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include "app/account_service.h"
#include "app/contact_service.h"

#include <QPoint>
#include <QWidget>

class QAction;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QToolButton;

namespace tincan::ui {

class ContactsView : public QWidget {
    Q_OBJECT

public:
    ContactsView(ContactService &contactService, AccountService &accountService,
                 QWidget *parent = nullptr);

    QLineEdit *searchEdit() const;
    QPushButton *addButton() const;
    QListWidget *listWidget() const;
    QAction *importCsvAction() const;
    QAction *exportCsvAction() const;

signals:
    void callRequested(const QString &number, const QString &accountId);
    void messageRequested(const QString &number, const QString &accountId);

private:
    QWidget *buildToolbar();
    void refreshList();
    void onAddClicked();
    void onItemDoubleClicked(QListWidgetItem *item);
    void onContextMenuRequested(const QPoint &pos);
    void editContactById(qint64 id);
    void onImportCsv();
    void onExportCsv();

    ContactService &contactService_;
    AccountService &accountService_;
    QLineEdit *searchEdit_ = nullptr;
    QPushButton *addButton_ = nullptr;
    QToolButton *csvButton_ = nullptr;
    QAction *importAction_ = nullptr;
    QAction *exportAction_ = nullptr;
    QListWidget *listWidget_ = nullptr;
};

} // namespace tincan::ui
