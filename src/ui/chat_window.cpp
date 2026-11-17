// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "ui/chat_window.h"

#include "core/sip_uri.h"
#include "ui/icons.h"

#include <QDate>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QShowEvent>
#include <QVBoxLayout>

#include <utility>

namespace tincan::ui {

namespace {

QString formatMsgTime(const QDateTime &utcDt) {
    const QDateTime local = utcDt.toLocalTime();
    if (local.date() == QDate::currentDate()) {
        return local.toString(QStringLiteral("HH:mm:ss"));
    }
    return local.toString(QStringLiteral("yyyy-MM-dd HH:mm"));
}

QString formatDeliveryState(MessageStatus status) {
    switch (status) {
    case MessageStatus::Pending:
        return ChatWindow::tr("Pending");
    case MessageStatus::Sent:
        return ChatWindow::tr("Sent");
    case MessageStatus::Failed:
        return ChatWindow::tr("Failed");
    case MessageStatus::Received:
        return {};
    }
    return {};
}

QString resolvePeerLabel(const QString &peerUri, const ContactService &contacts) {
    const QString name = contacts.resolveDisplayName(peerUri);
    if (!name.isEmpty()) {
        return name;
    }
    const QString user = SipUri::extractUserPart(peerUri);
    return user.isEmpty() ? peerUri : user;
}

} // namespace

ChatWindow::ChatWindow(QString peerUri, QString accountId, MessageService &messageService,
                       ContactService &contactService, QWidget *parent)
    : QWidget(parent, Qt::Window), peerUri_(std::move(peerUri)), accountId_(std::move(accountId)),
      messageService_(messageService), contactService_(contactService) {
    setupUi();

    connect(inputEdit_, &QLineEdit::returnPressed, this, &ChatWindow::sendCurrentText);
    connect(sendButton_, &QPushButton::clicked, this, &ChatWindow::sendCurrentText);

    connect(&messageService_, &MessageService::messagesChanged, this,
            [this](const QString &changedPeer) {
                if (changedPeer.compare(peerUri_, Qt::CaseInsensitive) == 0) {
                    if (isVisible() && messageService_.unreadCountForPeer(peerUri_) > 0) {
                        messageService_.markPeerRead(peerUri_);
                    }
                    refreshTranscript();
                }
            });
    connect(&contactService_, &ContactService::contactsChanged, this, [this]() {
        refreshHeader();
        refreshTranscript();
    });

    refreshHeader();
    refreshTranscript();
}

void ChatWindow::setupUi() {
    setObjectName(QStringLiteral("chatWindow"));
    setWindowIcon(Icons::appIcon());
    resize(340, 420);
    setMinimumSize(280, 300);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(6);

    headerLabel_ = new QLabel(this);
    headerLabel_->setObjectName(QStringLiteral("chatHeaderLabel"));
    headerLabel_->setWordWrap(true);
    headerLabel_->setStyleSheet(
        QStringLiteral("background-color: #f1f5f9; border: 1px solid #cbd5e1; "
                       "border-radius: 4px; padding: 6px; font-weight: bold;"));
    root->addWidget(headerLabel_);

    errorLabel_ = new QLabel(this);
    errorLabel_->setStyleSheet(QStringLiteral("color: #b91c1c; font-size: 11px;"));
    errorLabel_->setWordWrap(true);
    errorLabel_->setVisible(false);
    root->addWidget(errorLabel_);

    transcriptList_ = new QListWidget(this);
    transcriptList_->setObjectName(QStringLiteral("chatTranscriptList"));
    transcriptList_->setWordWrap(true);
    transcriptList_->setSpacing(4);
    transcriptList_->setSelectionMode(QAbstractItemView::NoSelection);
    root->addWidget(transcriptList_, 1);

    auto *inputRow = new QHBoxLayout();
    inputRow->setSpacing(4);
    inputEdit_ = new QLineEdit(this);
    inputEdit_->setObjectName(QStringLiteral("chatInputEdit"));
    inputEdit_->setPlaceholderText(tr("Type a message..."));

    sendButton_ = new QPushButton(tr("Send"), this);
    sendButton_->setObjectName(QStringLiteral("chatSendButton"));

    inputRow->addWidget(inputEdit_, 1);
    inputRow->addWidget(sendButton_);
    root->addLayout(inputRow);
}

const QString &ChatWindow::peerUri() const { return peerUri_; }

void ChatWindow::setAccountId(const QString &accountId) {
    if (!accountId.trimmed().isEmpty()) {
        accountId_ = accountId.trimmed();
    }
}

QListWidget *ChatWindow::transcriptList() const { return transcriptList_; }
QLineEdit *ChatWindow::inputEdit() const { return inputEdit_; }
QPushButton *ChatWindow::sendButton() const { return sendButton_; }

void ChatWindow::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    messageService_.markPeerRead(peerUri_);
    inputEdit_->setFocus();
}

void ChatWindow::changeEvent(QEvent *event) {
    QWidget::changeEvent(event);
    if (event->type() == QEvent::ActivationChange && isActiveWindow()) {
        messageService_.markPeerRead(peerUri_);
    }
}

void ChatWindow::refreshHeader() {
    const QString label = resolvePeerLabel(peerUri_, contactService_);
    setWindowTitle(tr("Chat — %1").arg(label));
    if (label != peerUri_) {
        headerLabel_->setText(QStringLiteral("%1\n%2").arg(label, peerUri_));
    } else {
        headerLabel_->setText(peerUri_);
    }
}

void ChatWindow::refreshTranscript() {
    transcriptList_->clear();
    const QString peerName = resolvePeerLabel(peerUri_, contactService_);
    const QList<ChatMessage> msgs = messageService_.messagesForPeer(peerUri_, 500);

    for (const ChatMessage &m : msgs) {
        const bool outgoing = (m.direction == MessageDirection::Outgoing);
        const QString who = outgoing ? tr("Me") : peerName;
        const QString timeStr = formatMsgTime(m.sentAt);
        const QString stateStr = outgoing ? formatDeliveryState(m.status) : QString{};
        const QString meta = stateStr.isEmpty()
                                 ? QStringLiteral("%1 · %2").arg(who, timeStr)
                                 : QStringLiteral("%1 · %2 · %3").arg(who, timeStr, stateStr);

        auto *item =
            new QListWidgetItem(QStringLiteral("%1\n%2").arg(meta, m.body), transcriptList_);
        if (outgoing) {
            item->setBackground(
                m.status == MessageStatus::Failed ? QColor(254, 242, 242) : QColor(239, 246, 255));
        } else {
            item->setBackground(QColor(248, 250, 252));
        }
        if (m.status == MessageStatus::Failed) {
            item->setForeground(QColor(185, 28, 28));
        }
    }
    transcriptList_->scrollToBottom();
}

void ChatWindow::sendCurrentText() {
    const QString text = inputEdit_->text();
    if (text.trimmed().isEmpty()) {
        return;
    }
    errorLabel_->setVisible(false);
    QString err;
    const qint64 id = messageService_.sendMessage(peerUri_, text, accountId_, &err);
    if (id <= 0) {
        errorLabel_->setText(err);
        errorLabel_->setVisible(!err.isEmpty());
        return;
    }
    inputEdit_->clear();
}

} // namespace tincan::ui
