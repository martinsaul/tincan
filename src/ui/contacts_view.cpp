// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "ui/contacts_view.h"

#include "ui/contact_dialog.h"
#include "ui/icons.h"

#include <QAction>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

namespace tincan::ui {

namespace {

constexpr int kRoleContactId = Qt::UserRole;
constexpr int kRoleNumber = Qt::UserRole + 1;
constexpr int kRoleAccountId = Qt::UserRole + 2;

QString presenceStateText(PresenceState state) {
    switch (state) {
    case PresenceState::Unknown:
        return {};
    case PresenceState::Offline:
        return ContactsView::tr("Offline");
    case PresenceState::Online:
        return ContactsView::tr("Online");
    case PresenceState::Away:
        return ContactsView::tr("Away");
    case PresenceState::Busy:
        return ContactsView::tr("Busy");
    case PresenceState::Ringing:
        return ContactsView::tr("Ringing");
    case PresenceState::OnThePhone:
        return ContactsView::tr("On the phone");
    }
    return {};
}

QString formatContactItemText(const Contact &c, const PresenceInfo &pres) {
    QString secondLine = c.number;
    if (c.presence) {
        const QString status = presenceStateText(pres.state);
        if (!status.isEmpty()) {
            secondLine.append(QStringLiteral(" · ") + status);
        }
    }
    return QStringLiteral("%1\n%2").arg(c.name, secondLine);
}

} // namespace

ContactsView::ContactsView(ContactService &contactService, AccountService &accountService,
                           QWidget *parent)
    : QWidget(parent), contactService_(contactService), accountService_(accountService) {
    setObjectName(QStringLiteral("contactsView"));

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(6);

    root->addWidget(buildToolbar());

    listWidget_ = new QListWidget(this);
    listWidget_->setObjectName(QStringLiteral("contactsList"));
    listWidget_->setContextMenuPolicy(Qt::CustomContextMenu);
    listWidget_->setAlternatingRowColors(true);
    listWidget_->setSpacing(2);
    root->addWidget(listWidget_, 1);

    connect(searchEdit_, &QLineEdit::textChanged, this, [this](const QString &) { refreshList(); });
    connect(listWidget_, &QListWidget::itemDoubleClicked, this,
            &ContactsView::onItemDoubleClicked);
    connect(listWidget_, &QWidget::customContextMenuRequested, this,
            &ContactsView::onContextMenuRequested);
    connect(&contactService_, &ContactService::contactsChanged, this, &ContactsView::refreshList);
    connect(&contactService_, &ContactService::presenceChanged, this,
            [this](const PresenceInfo &) { refreshList(); });

    refreshList();
}

QWidget *ContactsView::buildToolbar() {
    auto *bar = new QWidget(this);
    auto *row = new QHBoxLayout(bar);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(4);

    searchEdit_ = new QLineEdit(bar);
    searchEdit_->setObjectName(QStringLiteral("contactsSearchEdit"));
    searchEdit_->setPlaceholderText(tr("Search contacts..."));
    searchEdit_->setClearButtonEnabled(true);

    addButton_ = new QPushButton(tr("Add"), bar);
    addButton_->setObjectName(QStringLiteral("addContactButton"));
    connect(addButton_, &QPushButton::clicked, this, &ContactsView::onAddClicked);

    csvButton_ = new QToolButton(bar);
    csvButton_->setObjectName(QStringLiteral("contactsCsvButton"));
    csvButton_->setText(tr("CSV"));
    csvButton_->setPopupMode(QToolButton::InstantPopup);
    auto *csvMenu = new QMenu(csvButton_);
    importAction_ = csvMenu->addAction(tr("Import CSV..."));
    exportAction_ = csvMenu->addAction(tr("Export CSV..."));
    connect(importAction_, &QAction::triggered, this, &ContactsView::onImportCsv);
    connect(exportAction_, &QAction::triggered, this, &ContactsView::onExportCsv);
    csvButton_->setMenu(csvMenu);

    row->addWidget(searchEdit_, 1);
    row->addWidget(addButton_);
    row->addWidget(csvButton_);
    return bar;
}

QLineEdit *ContactsView::searchEdit() const { return searchEdit_; }
QPushButton *ContactsView::addButton() const { return addButton_; }
QListWidget *ContactsView::listWidget() const { return listWidget_; }
QAction *ContactsView::importCsvAction() const { return importAction_; }
QAction *ContactsView::exportCsvAction() const { return exportAction_; }

void ContactsView::refreshList() {
    listWidget_->clear();
    const QList<Contact> items = contactService_.search(searchEdit_->text());
    for (const Contact &c : items) {
        const PresenceInfo pres =
            c.presence ? contactService_.presenceForContact(c) : PresenceInfo{};
        auto *item = new QListWidgetItem(Icons::presenceDotIcon(pres.state, 10),
                                         formatContactItemText(c, pres), listWidget_);
        item->setData(kRoleContactId, c.id);
        item->setData(kRoleNumber, c.number);
        item->setData(kRoleAccountId, c.accountId);
        if (!pres.note.isEmpty()) {
            item->setToolTip(pres.note);
        }
    }
}

void ContactsView::onAddClicked() {
    ContactDialog dlg(accountService_.accounts(), window());
    if (dlg.exec() == QDialog::Accepted) {
        (void)contactService_.addContact(dlg.contact());
    }
}

void ContactsView::onItemDoubleClicked(QListWidgetItem *item) {
    if (item == nullptr) {
        return;
    }
    emit callRequested(item->data(kRoleNumber).toString(), item->data(kRoleAccountId).toString());
}

void ContactsView::onContextMenuRequested(const QPoint &pos) {
    QListWidgetItem *item = listWidget_->itemAt(pos);
    if (item == nullptr) {
        return;
    }
    const qint64 id = item->data(kRoleContactId).toLongLong();
    const QString number = item->data(kRoleNumber).toString();
    const QString accId = item->data(kRoleAccountId).toString();

    QMenu menu(this);
    QAction *callAct = menu.addAction(Icons::callStart(), tr("Call"));
    QAction *msgAct = menu.addAction(tr("Message"));
    menu.addSeparator();
    QAction *editAct = menu.addAction(tr("Edit"));
    QAction *delAct = menu.addAction(tr("Delete"));

    QAction *chosen = menu.exec(listWidget_->viewport()->mapToGlobal(pos));
    if (chosen == callAct) {
        emit callRequested(number, accId);
    } else if (chosen == msgAct) {
        emit messageRequested(number, accId);
    } else if (chosen == editAct) {
        editContactById(id);
    } else if (chosen == delAct) {
        (void)contactService_.removeContact(id);
    }
}

void ContactsView::editContactById(qint64 id) {
    const auto existing = contactService_.contact(id);
    if (!existing.has_value()) {
        return;
    }
    ContactDialog dlg(*existing, accountService_.accounts(), window());
    if (dlg.exec() == QDialog::Accepted) {
        (void)contactService_.updateContact(dlg.contact());
    }
}

void ContactsView::onImportCsv() {
    const QString path = QFileDialog::getOpenFileName(
        window(), tr("Import Contacts CSV"), QString{}, tr("CSV files (*.csv);;All files (*)"));
    if (path.isEmpty()) {
        return;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(window(), tr("Import Failed"), tr("Could not open selected CSV file."));
        return;
    }
    const auto res = contactService_.importCsv(QString::fromUtf8(file.readAll()));
    QMessageBox::information(
        window(), tr("Contacts Imported"),
        tr("Imported %1 contacts (%2 duplicates skipped).").arg(res.imported).arg(res.skippedDuplicates));
}

void ContactsView::onExportCsv() {
    const QString path = QFileDialog::getSaveFileName(
        window(), tr("Export Contacts CSV"), QStringLiteral("contacts.csv"),
        tr("CSV files (*.csv);;All files (*)"));
    if (path.isEmpty()) {
        return;
    }
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::warning(window(), tr("Export Failed"), tr("Could not write CSV file."));
        return;
    }
    file.write(contactService_.exportCsv().toUtf8());
}

} // namespace tincan::ui
