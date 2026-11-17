// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include "app/call_service.h"
#include "app/history_repository.h"
#include "core/history_entry.h"

#include <QDateTime>
#include <QHash>
#include <QList>
#include <QObject>
#include <QString>

namespace tincan {

class ContactService;

class HistoryService : public QObject {
    Q_OBJECT

public:
    HistoryService(CallService &callService, HistoryRepository &repository,
                   const ContactService *contactService = nullptr, QObject *parent = nullptr);

    QList<HistoryEntry> search(const QString &text = {}, HistoryFilter filter = HistoryFilter::All,
                               int limit = 200) const;
    int unseenMissedCount() const;
    void markAllSeen();
    bool removeEntry(qint64 id);
    void clearAll();

signals:
    void historyChanged();
    void missedCountChanged(int count);

private:
    struct TrackedCall {
        QDateTime startedAt;
        QDateTime connectedAt;
        CallInfo lastInfo;
    };

    void onCallAddedOrUpdated(const CallInfo &info);
    void onCallEnded(const CallInfo &info, bool missed);
    QString resolveRemoteName(const CallInfo &info) const;
    void notifyCountIfChanged();

    CallService &callService_;
    HistoryRepository &repository_;
    const ContactService *contactService_ = nullptr;
    QHash<int, TrackedCall> trackedCalls_;
    int cachedMissedCount_ = 0;
};

} // namespace tincan
