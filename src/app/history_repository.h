// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include "app/database.h"
#include "core/history_entry.h"

#include <QList>
#include <QString>

#include <optional>

namespace tincan {

class HistoryRepository {
public:
    explicit HistoryRepository(Database &database);

    qint64 insert(const HistoryEntry &entry);
    bool remove(qint64 id);
    std::optional<HistoryEntry> findById(qint64 id) const;
    QList<HistoryEntry> search(const QString &text = {}, HistoryFilter filter = HistoryFilter::All,
                               int limit = 200) const;
    bool markAllSeen();
    int unseenMissedCount() const;
    bool clear();

private:
    Database &database_;
};

} // namespace tincan
