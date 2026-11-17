// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include "app/call_service.h"
#include "app/contact_service.h"
#include "app/message_service.h"
#include "app/notification_backend.h"
#include "app/settings_store.h"

#include <QHash>
#include <QObject>
#include <QString>

#include <functional>

namespace tincan {

// Announces incoming calls, missed calls and new messages as desktop
// notifications. When no notification service exists it asks the UI to show
// a fallback message instead (fallbackRequested).
class Notifier : public QObject {
    Q_OBJECT

public:
    using MessageFilter = std::function<bool(const ChatMessage &message)>;

    Notifier(CallService &callService, SettingsStore &settings, INotificationBackend &backend,
             MessageService *messageService = nullptr, ContactService *contactService = nullptr,
             QObject *parent = nullptr);

    // Messages for which the filter returns true are not announced (e.g. the
    // conversation is already on screen).
    void setMessageSuppressor(MessageFilter filter);

signals:
    void fallbackRequested(const QString &title, const QString &body);
    void openHistoryRequested();
    void openChatRequested(const QString &peerUri, const QString &accountId);

private:
    enum class Kind { IncomingCall, MissedCall, Message };
    struct Entry {
        Kind kind = Kind::Message;
        int callId = -1;
        QString peerUri;
        QString accountId;
    };

    void onIncomingCall(const CallInfo &info);
    void onCallsChanged();
    void onMissedCall(const CallInfo &info);
    void onIncomingMessage(const ChatMessage &message, const QString &fromName);
    void onActionInvoked(uint id, const QString &actionKey);
    void onNotificationClosed(uint id);
    void post(const NotificationRequest &request, const Entry &entry, bool fallback);
    QString callerName(const QString &uri, const QString &displayName) const;

    CallService &callService_;
    SettingsStore &settings_;
    INotificationBackend &backend_;
    ContactService *contactService_ = nullptr;
    MessageFilter messageSuppressor_;
    QHash<uint, Entry> entries_;
};

} // namespace tincan
