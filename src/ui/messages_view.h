// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include "app/account_service.h"
#include "app/contact_service.h"
#include "app/message_service.h"

#include <QMap>
#include <QPoint>
#include <QString>
#include <QWidget>

class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPushButton;

namespace tincan::ui {

class ChatWindow;

class MessagesView : public QWidget {
    Q_OBJECT

public:
    MessagesView(MessageService &messageService, ContactService &contactService,
                 AccountService &accountService, QWidget *parent = nullptr);
    ~MessagesView() override;

    ChatWindow *openChatForPeer(const QString &peerOrNumber, const QString &accountId = {});
    ChatWindow *chatWindowForPeer(const QString &peerOrNumber) const;
    bool isPeerChatVisible(const QString &peerOrNumber) const;

    QLineEdit *newTargetEdit() const;
    QPushButton *startChatButton() const;
    QListWidget *conversationList() const;

signals:
    void callRequested(const QString &uri, const QString &accountId);

private:
    QWidget *buildToolbar();
    void refreshConversations();
    void onStartChat();
    void onItemActivated(QListWidgetItem *item);
    void onContextMenuRequested(const QPoint &pos);
    void addPeerToContacts(const QString &peerUri, const QString &accountId);

    MessageService &messageService_;
    ContactService &contactService_;
    AccountService &accountService_;

    QLineEdit *newTargetEdit_ = nullptr;
    QPushButton *startChatButton_ = nullptr;
    QListWidget *conversationList_ = nullptr;
    QMap<QString, ChatWindow *> chatWindows_;
};

} // namespace tincan::ui
