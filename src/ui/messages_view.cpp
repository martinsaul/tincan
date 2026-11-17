// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "ui/messages_view.h"

#include "core/sip_uri.h"
#include "ui/chat_window.h"
#include "ui/contact_dialog.h"
#include "ui/icons.h"

#include <QDate>
#include <QFont>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPushButton>
#include <QVBoxLayout>

namespace tincan::ui {

namespace {

constexpr int kRolePeerUri = Qt::UserRole;
constexpr int kRoleAccountId = Qt::UserRole + 1;
constexpr int kRoleUnreadCount = Qt::UserRole + 2;

QString formatConvTime(const QDateTime &utcDt) {
    const QDateTime local = utcDt.toLocalTime();
    if (local.date() == QDate::currentDate()) {
        return local.toString(QStringLiteral("HH:mm"));
    }
    return local.toString(QStringLiteral("MM-dd"));
}

QString singleLinePreview(const QString &body) {
    QString clean = body.simplified();
    if (clean.size() > 48) {
        clean = clean.left(45) + QStringLiteral("...");
    }
    return clean;
}

QString resolveConvTitle(const ConversationSummary &conv, const ContactService &contacts) {
    const QString name = contacts.resolveDisplayName(conv.peerUri);
    if (!name.isEmpty()) {
        return name;
    }
    const QString user = SipUri::extractUserPart(conv.peerUri);
    return user.isEmpty() ? conv.peerUri : user;
}

} // namespace

MessagesView::MessagesView(MessageService &messageService, ContactService &contactService,
                           AccountService &accountService, QWidget *parent)
    : QWidget(parent), messageService_(messageService), contactService_(contactService),
      accountService_(accountService) {
    setObjectName(QStringLiteral("messagesView"));

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(6);

    root->addWidget(buildToolbar());

    conversationList_ = new QListWidget(this);
    conversationList_->setObjectName(QStringLiteral("conversationsList"));
    conversationList_->setContextMenuPolicy(Qt::CustomContextMenu);
    conversationList_->setAlternatingRowColors(true);
    conversationList_->setSpacing(2);
    root->addWidget(conversationList_, 1);

    connect(newTargetEdit_, &QLineEdit::returnPressed, this, &MessagesView::onStartChat);
    connect(startChatButton_, &QPushButton::clicked, this, &MessagesView::onStartChat);
    connect(conversationList_, &QListWidget::itemClicked, this, &MessagesView::onItemActivated);
    connect(conversationList_, &QListWidget::itemDoubleClicked, this,
            &MessagesView::onItemActivated);
    connect(conversationList_, &QWidget::customContextMenuRequested, this,
            &MessagesView::onContextMenuRequested);
    connect(&messageService_, &MessageService::conversationsChanged, this,
            &MessagesView::refreshConversations);
    connect(&contactService_, &ContactService::contactsChanged, this,
            &MessagesView::refreshConversations);

    refreshConversations();
}

MessagesView::~MessagesView() {
    qDeleteAll(chatWindows_);
    chatWindows_.clear();
}

QWidget *MessagesView::buildToolbar() {
    auto *bar = new QWidget(this);
    auto *row = new QHBoxLayout(bar);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(4);

    newTargetEdit_ = new QLineEdit(bar);
    newTargetEdit_->setObjectName(QStringLiteral("newChatTargetEdit"));
    newTargetEdit_->setPlaceholderText(tr("Number or SIP URI..."));
    newTargetEdit_->setClearButtonEnabled(true);

    startChatButton_ = new QPushButton(tr("Chat"), bar);
    startChatButton_->setObjectName(QStringLiteral("startChatButton"));

    row->addWidget(newTargetEdit_, 1);
    row->addWidget(startChatButton_);
    return bar;
}

ChatWindow *MessagesView::openChatForPeer(const QString &peerOrNumber, const QString &accountId) {
    const QString canonical = messageService_.resolvePeerUri(peerOrNumber, accountId);
    if (canonical.isEmpty()) {
        return nullptr;
    }
    const QString key = canonical.toLower();
    ChatWindow *win = chatWindows_.value(key, nullptr);
    if (win == nullptr) {
        win = new ChatWindow(canonical, accountId, messageService_, contactService_);
        chatWindows_.insert(key, win);
    } else {
        win->setAccountId(accountId);
    }
    win->showNormal();
    win->raise();
    win->activateWindow();
    messageService_.markPeerRead(canonical);
    return win;
}

ChatWindow *MessagesView::chatWindowForPeer(const QString &peerOrNumber) const {
    const QString canonical = messageService_.resolvePeerUri(peerOrNumber);
    return chatWindows_.value(canonical.toLower(), nullptr);
}

bool MessagesView::isPeerChatVisible(const QString &peerOrNumber) const {
    const ChatWindow *win = chatWindowForPeer(peerOrNumber);
    return win != nullptr && win->isVisible();
}

QLineEdit *MessagesView::newTargetEdit() const { return newTargetEdit_; }
QPushButton *MessagesView::startChatButton() const { return startChatButton_; }
QListWidget *MessagesView::conversationList() const { return conversationList_; }

void MessagesView::refreshConversations() {
    conversationList_->clear();
    const QList<ConversationSummary> list = messageService_.conversations();

    for (const ConversationSummary &conv : list) {
        const QString title = resolveConvTitle(conv, contactService_);
        const QString timeText = formatConvTime(conv.lastSentAt);
        const QString badge = conv.unreadCount > 0
                                  ? QStringLiteral("  [%1]").arg(conv.unreadCount)
                                  : QString{};
        const QString header = QStringLiteral("%1 · %2%3").arg(title, timeText, badge);
        auto *item = new QListWidgetItem(
            QStringLiteral("%1\n%2").arg(header, singleLinePreview(conv.lastBody)),
            conversationList_);
        item->setData(kRolePeerUri, conv.peerUri);
        item->setData(kRoleAccountId, conv.accountId);
        item->setData(kRoleUnreadCount, conv.unreadCount);
        item->setToolTip(conv.peerUri);

        if (conv.unreadCount > 0) {
            QFont boldFont = item->font();
            boldFont.setBold(true);
            item->setFont(boldFont);
            item->setForeground(QColor(29, 78, 216));
        }
    }
}

void MessagesView::onStartChat() {
    const QString target = newTargetEdit_->text().trimmed();
    if (target.isEmpty()) {
        return;
    }
    if (openChatForPeer(target) != nullptr) {
        newTargetEdit_->clear();
    }
}

void MessagesView::onItemActivated(QListWidgetItem *item) {
    if (item == nullptr) {
        return;
    }
    (void)openChatForPeer(item->data(kRolePeerUri).toString(),
                          item->data(kRoleAccountId).toString());
}

void MessagesView::onContextMenuRequested(const QPoint &pos) {
    QListWidgetItem *item = conversationList_->itemAt(pos);
    if (item == nullptr) {
        return;
    }
    const QString peerUri = item->data(kRolePeerUri).toString();
    const QString accId = item->data(kRoleAccountId).toString();

    QMenu menu(this);
    QAction *openAct = menu.addAction(tr("Open Chat"));
    QAction *callAct = menu.addAction(Icons::callStart(), tr("Call"));
    QAction *addAct = menu.addAction(tr("Add to contacts..."));
    menu.addSeparator();
    QAction *delAct = menu.addAction(tr("Delete conversation"));

    QAction *chosen = menu.exec(conversationList_->viewport()->mapToGlobal(pos));
    if (chosen == openAct) {
        (void)openChatForPeer(peerUri, accId);
    } else if (chosen == callAct) {
        emit callRequested(peerUri, accId);
    } else if (chosen == addAct) {
        addPeerToContacts(peerUri, accId);
    } else if (chosen == delAct) {
        if (ChatWindow *win = chatWindows_.take(peerUri.toLower())) {
            win->close();
            win->deleteLater();
        }
        (void)messageService_.deleteConversation(peerUri);
    }
}

void MessagesView::addPeerToContacts(const QString &peerUri, const QString &accountId) {
    Contact initial;
    const QString user = SipUri::extractUserPart(peerUri);
    initial.number = user.isEmpty() ? peerUri : user;
    initial.accountId = accountId;

    ContactDialog dlg(initial, accountService_.accounts(), window());
    dlg.setWindowTitle(tr("Add Contact"));
    if (dlg.exec() == QDialog::Accepted) {
        (void)contactService_.addContact(dlg.contact());
    }
}

} // namespace tincan::ui
