// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "app/history_service.h"

#include "app/contact_service.h"

namespace tincan {

HistoryService::HistoryService(CallService &callService, HistoryRepository &repository,
                               const ContactService *contactService, QObject *parent)
    : QObject(parent), callService_(callService), repository_(repository),
      contactService_(contactService), cachedMissedCount_(repository.unseenMissedCount()) {
    connect(&callService_, &CallService::callAdded, this, &HistoryService::onCallAddedOrUpdated);
    connect(&callService_, &CallService::callUpdated, this, &HistoryService::onCallAddedOrUpdated);
    connect(&callService_, &CallService::callEnded, this, &HistoryService::onCallEnded);
}

QList<HistoryEntry> HistoryService::search(const QString &text, HistoryFilter filter,
                                           int limit) const {
    return repository_.search(text, filter, limit);
}

int HistoryService::unseenMissedCount() const {
    return repository_.unseenMissedCount();
}

void HistoryService::markAllSeen() {
    if (!repository_.markAllSeen()) {
        return;
    }
    notifyCountIfChanged();
    emit historyChanged();
}

bool HistoryService::removeEntry(qint64 id) {
    if (!repository_.remove(id)) {
        return false;
    }
    notifyCountIfChanged();
    emit historyChanged();
    return true;
}

void HistoryService::clearAll() {
    if (!repository_.clear()) {
        return;
    }
    notifyCountIfChanged();
    emit historyChanged();
}

void HistoryService::onCallAddedOrUpdated(const CallInfo &info) {
    auto it = trackedCalls_.find(info.callId);
    if (it == trackedCalls_.end()) {
        TrackedCall tracked;
        tracked.startedAt = QDateTime::currentDateTimeUtc();
        if (info.connectedAt.isValid()) {
            tracked.connectedAt = info.connectedAt.toUTC();
            if (tracked.connectedAt < tracked.startedAt) {
                tracked.startedAt = tracked.connectedAt;
            }
        }
        tracked.lastInfo = info;
        trackedCalls_.insert(info.callId, tracked);
        return;
    }

    if (info.connectedAt.isValid() && !it->connectedAt.isValid()) {
        it->connectedAt = info.connectedAt.toUTC();
    } else if (info.state == CallState::Confirmed && !it->connectedAt.isValid()) {
        it->connectedAt = QDateTime::currentDateTimeUtc();
    }
    it->lastInfo = info;
}

void HistoryService::onCallEnded(const CallInfo &info, bool missed) {
    const QDateTime nowUtc = QDateTime::currentDateTimeUtc();
    const bool hadTracked = trackedCalls_.contains(info.callId);
    const TrackedCall tracked = trackedCalls_.take(info.callId);

    const CallInfo effective = info.remoteUri.isEmpty() && hadTracked ? tracked.lastInfo : info;
    if (effective.remoteUri.isEmpty()) {
        return;
    }

    HistoryEntry entry;
    entry.accountId =
        effective.accountId.isEmpty() && hadTracked ? tracked.lastInfo.accountId : effective.accountId;
    entry.direction = effective.direction;
    entry.remoteUri = effective.remoteUri;
    entry.remoteName = resolveRemoteName(effective);
    entry.startedAt = hadTracked && tracked.startedAt.isValid() ? tracked.startedAt : nowUtc;
    if (info.connectedAt.isValid()) {
        entry.connectedAt = info.connectedAt.toUTC();
    } else if (hadTracked && tracked.connectedAt.isValid()) {
        entry.connectedAt = tracked.connectedAt;
    }
    if (entry.connectedAt.isValid() && entry.connectedAt < entry.startedAt) {
        entry.startedAt = entry.connectedAt;
    }
    entry.endedAt = nowUtc;
    entry.statusCode = info.lastStatusCode;
    entry.missed = missed;
    entry.seen = !missed;

    if (repository_.insert(entry) > 0) {
        notifyCountIfChanged();
        emit historyChanged();
    }
}

QString HistoryService::resolveRemoteName(const CallInfo &info) const {
    if (contactService_ != nullptr) {
        const QString contactName = contactService_->resolveDisplayName(info.remoteUri);
        if (!contactName.isEmpty()) {
            return contactName;
        }
    }
    return info.remoteDisplayName.trimmed();
}

void HistoryService::notifyCountIfChanged() {
    const int current = repository_.unseenMissedCount();
    if (current != cachedMissedCount_) {
        cachedMissedCount_ = current;
        emit missedCountChanged(current);
    }
}

} // namespace tincan
