// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "ui/history_view.h"

#include "core/sip_uri.h"
#include "ui/contact_dialog.h"
#include "ui/icons.h"

#include <QComboBox>
#include <QDate>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QVBoxLayout>

namespace tincan::ui {

namespace {

constexpr int kRoleHistoryId = Qt::UserRole;
constexpr int kRoleRemoteUri = Qt::UserRole + 1;
constexpr int kRoleAccountId = Qt::UserRole + 2;
constexpr int kRoleRemoteName = Qt::UserRole + 3;

QString formatTimestamp(const QDateTime &utcDt) {
    const QDateTime local = utcDt.toLocalTime();
    if (local.date() == QDate::currentDate()) {
        return local.toString(QStringLiteral("HH:mm"));
    }
    return local.toString(QStringLiteral("yyyy-MM-dd HH:mm"));
}

QString formatDurationOrOutcome(const HistoryEntry &e) {
    if (e.connectedAt.isValid() && e.endedAt.isValid()) {
        const qint64 secs = qMax<qint64>(0, e.connectedAt.secsTo(e.endedAt));
        return QStringLiteral("%1:%2")
            .arg(secs / 60, 2, 10, QLatin1Char('0'))
            .arg(secs % 60, 2, 10, QLatin1Char('0'));
    }
    if (e.missed) {
        return HistoryView::tr("Missed");
    }
    if (e.statusCode > 0) {
        return HistoryView::tr("No answer (%1)").arg(e.statusCode);
    }
    return HistoryView::tr("Unanswered");
}

QString resolveEntryDisplay(const HistoryEntry &e, const ContactService &contacts) {
    const QString byContact = contacts.resolveDisplayName(e.remoteUri);
    if (!byContact.isEmpty()) {
        return byContact;
    }
    if (!e.remoteName.trimmed().isEmpty()) {
        return e.remoteName.trimmed();
    }
    const QString user = SipUri::extractUserPart(e.remoteUri);
    return user.isEmpty() ? e.remoteUri : user;
}

} // namespace

HistoryView::HistoryView(HistoryService &historyService, ContactService &contactService,
                         AccountService &accountService, QWidget *parent)
    : QWidget(parent), historyService_(historyService), contactService_(contactService),
      accountService_(accountService) {
    setObjectName(QStringLiteral("historyView"));

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(6);

    root->addWidget(buildToolbar());

    listWidget_ = new QListWidget(this);
    listWidget_->setObjectName(QStringLiteral("historyList"));
    listWidget_->setContextMenuPolicy(Qt::CustomContextMenu);
    listWidget_->setAlternatingRowColors(true);
    listWidget_->setSpacing(2);
    root->addWidget(listWidget_, 1);

    connect(filterCombo_, &QComboBox::currentIndexChanged, this,
            [this](int) { refreshList(); });
    connect(searchEdit_, &QLineEdit::textChanged, this, [this](const QString &) { refreshList(); });
    connect(listWidget_, &QListWidget::itemDoubleClicked, this,
            &HistoryView::onItemDoubleClicked);
    connect(listWidget_, &QWidget::customContextMenuRequested, this,
            &HistoryView::onContextMenuRequested);
    connect(&historyService_, &HistoryService::historyChanged, this, &HistoryView::refreshList);
    connect(&contactService_, &ContactService::contactsChanged, this, &HistoryView::refreshList);

    refreshList();
}

QWidget *HistoryView::buildToolbar() {
    auto *bar = new QWidget(this);
    auto *row = new QHBoxLayout(bar);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(4);

    filterCombo_ = new QComboBox(bar);
    filterCombo_->setObjectName(QStringLiteral("historyFilterCombo"));
    filterCombo_->addItem(tr("All"), static_cast<int>(HistoryFilter::All));
    filterCombo_->addItem(tr("Missed"), static_cast<int>(HistoryFilter::Missed));
    filterCombo_->addItem(tr("Incoming"), static_cast<int>(HistoryFilter::In));
    filterCombo_->addItem(tr("Outgoing"), static_cast<int>(HistoryFilter::Out));

    searchEdit_ = new QLineEdit(bar);
    searchEdit_->setObjectName(QStringLiteral("historySearchEdit"));
    searchEdit_->setPlaceholderText(tr("Search history..."));
    searchEdit_->setClearButtonEnabled(true);

    row->addWidget(filterCombo_);
    row->addWidget(searchEdit_, 1);
    return bar;
}

QComboBox *HistoryView::filterCombo() const { return filterCombo_; }
QLineEdit *HistoryView::searchEdit() const { return searchEdit_; }
QListWidget *HistoryView::listWidget() const { return listWidget_; }

void HistoryView::refreshList() {
    listWidget_->clear();
    const auto filter = static_cast<HistoryFilter>(filterCombo_->currentData().toInt());
    const QList<HistoryEntry> entries = historyService_.search(searchEdit_->text(), filter, 200);

    for (const HistoryEntry &e : entries) {
        const QString title = resolveEntryDisplay(e, contactService_);
        const QString subtitle =
            QStringLiteral("%1 · %2").arg(formatTimestamp(e.startedAt), formatDurationOrOutcome(e));
        auto *item =
            new QListWidgetItem(Icons::historyDirectionIcon(e.direction, e.missed),
                                QStringLiteral("%1\n%2").arg(title, subtitle), listWidget_);
        item->setData(kRoleHistoryId, e.id);
        item->setData(kRoleRemoteUri, e.remoteUri);
        item->setData(kRoleAccountId, e.accountId);
        item->setData(kRoleRemoteName, e.remoteName);
        item->setToolTip(e.remoteUri);
        if (e.missed) {
            item->setForeground(QColor(185, 28, 28));
        }
    }
}

void HistoryView::onItemDoubleClicked(QListWidgetItem *item) {
    if (item == nullptr) {
        return;
    }
    emit callRequested(item->data(kRoleRemoteUri).toString(),
                       item->data(kRoleAccountId).toString());
}

void HistoryView::onContextMenuRequested(const QPoint &pos) {
    QListWidgetItem *item = listWidget_->itemAt(pos);
    QMenu menu(this);

    QAction *callAct = nullptr;
    QAction *msgAct = nullptr;
    QAction *addContactAct = nullptr;
    QAction *delAct = nullptr;

    if (item != nullptr) {
        callAct = menu.addAction(Icons::callStart(), tr("Call"));
        msgAct = menu.addAction(tr("Message"));
        addContactAct = menu.addAction(tr("Add to contacts..."));
        menu.addSeparator();
        delAct = menu.addAction(tr("Delete"));
    }
    QAction *clearAct = menu.addAction(tr("Clear all"));

    QAction *chosen = menu.exec(listWidget_->viewport()->mapToGlobal(pos));
    if (chosen == nullptr) {
        return;
    }
    if (chosen == clearAct) {
        historyService_.clearAll();
        return;
    }
    if (item == nullptr) {
        return;
    }

    const QString uri = item->data(kRoleRemoteUri).toString();
    const QString accId = item->data(kRoleAccountId).toString();
    if (chosen == callAct) {
        emit callRequested(uri, accId);
    } else if (chosen == msgAct) {
        emit messageRequested(uri, accId);
    } else if (chosen == addContactAct) {
        addEntryToContacts(uri, item->data(kRoleRemoteName).toString(), accId);
    } else if (chosen == delAct) {
        (void)historyService_.removeEntry(item->data(kRoleHistoryId).toLongLong());
    }
}

void HistoryView::addEntryToContacts(const QString &uri, const QString &name,
                                     const QString &accountId) {
    Contact initial;
    initial.name = name;
    const QString user = SipUri::extractUserPart(uri);
    initial.number = user.isEmpty() ? uri : user;
    initial.accountId = accountId;

    ContactDialog dlg(initial, accountService_.accounts(), window());
    dlg.setWindowTitle(tr("Add Contact"));
    if (dlg.exec() == QDialog::Accepted) {
        (void)contactService_.addContact(dlg.contact());
    }
}

} // namespace tincan::ui
