// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include "app/contact_service.h"
#include "app/message_service.h"

#include <QString>
#include <QWidget>

class QEvent;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QShowEvent;

namespace tincan::ui {

class ChatWindow : public QWidget {
    Q_OBJECT

public:
    ChatWindow(QString peerUri, QString accountId, MessageService &messageService,
               ContactService &contactService, QWidget *parent = nullptr);

    const QString &peerUri() const;
    void setAccountId(const QString &accountId);

    QListWidget *transcriptList() const;
    QLineEdit *inputEdit() const;
    QPushButton *sendButton() const;

protected:
    void showEvent(QShowEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    void setupUi();
    void refreshHeader();
    void refreshTranscript();
    void sendCurrentText();

    QString peerUri_;
    QString accountId_;
    MessageService &messageService_;
    ContactService &contactService_;

    QLabel *headerLabel_ = nullptr;
    QLabel *errorLabel_ = nullptr;
    QListWidget *transcriptList_ = nullptr;
    QLineEdit *inputEdit_ = nullptr;
    QPushButton *sendButton_ = nullptr;
};

} // namespace tincan::ui
