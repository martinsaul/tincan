// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "app/notifier.h"

#include <QList>

#include <utility>

namespace tincan {

namespace {

const QString kAnswer = QStringLiteral("answer");
const QString kReject = QStringLiteral("reject");
const QString kOpen = QStringLiteral("open");
const QString kDefault = QStringLiteral("default");

bool wantsDesktopNotification(IncomingAlertMode mode) {
    return mode == IncomingAlertMode::Notification || mode == IncomingAlertMode::Both;
}

} // namespace

Notifier::Notifier(CallService &callService, SettingsStore &settings,
                   INotificationBackend &backend, MessageService *messageService,
                   ContactService *contactService, QObject *parent)
    : QObject(parent), callService_(callService), settings_(settings), backend_(backend),
      contactService_(contactService) {
    connect(&callService_, &CallService::incomingCall, this, &Notifier::onIncomingCall);
    connect(&callService_, &CallService::callUpdated, this,
            [this](const CallInfo &) { onCallsChanged(); });
    connect(&callService_, &CallService::callRemoved, this,
            [this](const CallInfo &) { onCallsChanged(); });
    connect(&callService_, &CallService::missedCall, this, &Notifier::onMissedCall);
    if (messageService != nullptr) {
        connect(messageService, &MessageService::incomingMessageReceived, this,
                &Notifier::onIncomingMessage);
    }
    connect(&backend_, &INotificationBackend::actionInvoked, this, &Notifier::onActionInvoked);
    connect(&backend_, &INotificationBackend::notificationClosed, this,
            &Notifier::onNotificationClosed);
}

void Notifier::setMessageSuppressor(MessageFilter filter) {
    messageSuppressor_ = std::move(filter);
}

QString Notifier::callerName(const QString &uri, const QString &displayName) const {
    if (contactService_ != nullptr) {
        const QString resolved = contactService_->resolveDisplayName(uri);
        if (!resolved.isEmpty()) {
            return resolved;
        }
    }
    return displayName.trimmed().isEmpty() ? uri : displayName.trimmed();
}

void Notifier::post(const NotificationRequest &request, const Entry &entry, bool fallback) {
    if (backend_.isAvailable()) {
        const uint id = backend_.notify(request);
        if (id != 0) {
            entries_.insert(id, entry);
            return;
        }
    }
    if (fallback) {
        emit fallbackRequested(request.summary, request.body);
    }
}

void Notifier::onIncomingCall(const CallInfo &info) {
    const IncomingAlertMode mode = settings_.generalSettings().incomingAlert;
    if (!wantsDesktopNotification(mode)) {
        return;
    }
    NotificationRequest request;
    request.summary = tr("Incoming call");
    request.body = callerName(info.remoteUri, info.remoteDisplayName);
    request.actions = {kAnswer, tr("Answer"), kReject, tr("Reject")};
    request.category = QStringLiteral("call.incoming");
    request.urgency = NotificationRequest::Urgency::Critical;
    request.persistent = true;
    // With "both", the popup already offers Answer/Reject if D-Bus is missing.
    post(request, Entry{Kind::IncomingCall, info.callId, info.remoteUri, info.accountId},
         mode == IncomingAlertMode::Notification);
}

void Notifier::onCallsChanged() {
    QList<uint> stale;
    for (auto it = entries_.cbegin(); it != entries_.cend(); ++it) {
        if (it.value().kind != Kind::IncomingCall) {
            continue;
        }
        bool stillRinging = false;
        for (const CallInfo &call : callService_.calls()) {
            stillRinging |= call.callId == it.value().callId && call.isIncomingRinging();
        }
        if (!stillRinging) {
            stale.append(it.key());
        }
    }
    for (const uint id : stale) {
        entries_.remove(id);
        backend_.close(id);
    }
}

void Notifier::onMissedCall(const CallInfo &info) {
    NotificationRequest request;
    request.summary = tr("Missed call");
    request.body =
        tr("Missed call from %1").arg(callerName(info.remoteUri, info.remoteDisplayName));
    request.actions = {kDefault, tr("Open"), kOpen, tr("Open")};
    request.category = QStringLiteral("call.unanswered");
    post(request, Entry{Kind::MissedCall, info.callId, info.remoteUri, info.accountId}, true);
}

void Notifier::onIncomingMessage(const ChatMessage &message, const QString &fromName) {
    if (messageSuppressor_ && messageSuppressor_(message)) {
        return;
    }
    NotificationRequest request;
    request.summary = tr("New message from %1").arg(callerName(message.peerUri, fromName));
    request.body = message.body;
    request.actions = {kDefault, tr("Open"), kOpen, tr("Open")};
    request.category = QStringLiteral("im.received");
    post(request, Entry{Kind::Message, -1, message.peerUri, message.accountId}, true);
}

void Notifier::onActionInvoked(uint id, const QString &actionKey) {
    const auto it = entries_.constFind(id);
    if (it == entries_.cend()) {
        return;
    }
    const Entry entry = it.value();
    if (entry.kind == Kind::IncomingCall) {
        if (actionKey == kAnswer) {
            (void)callService_.answer(entry.callId);
        } else if (actionKey == kReject) {
            (void)callService_.reject(entry.callId);
        }
        return;
    }
    if (actionKey != kOpen && actionKey != kDefault) {
        return;
    }
    if (entry.kind == Kind::MissedCall) {
        emit openHistoryRequested();
    } else {
        emit openChatRequested(entry.peerUri, entry.accountId);
    }
}

void Notifier::onNotificationClosed(uint id) { entries_.remove(id); }

} // namespace tincan
