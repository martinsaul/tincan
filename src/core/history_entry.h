// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include "core/call_info.h"

#include <QDateTime>
#include <QMetaType>
#include <QString>

namespace tincan {

enum class HistoryFilter {
    All = 0,
    Missed = 1,
    In = 2,
    Out = 3,
};

struct HistoryEntry {
    qint64 id = 0;
    QString accountId;
    CallDirection direction = CallDirection::Outgoing;
    QString remoteUri;
    QString remoteName;
    QDateTime startedAt;
    QDateTime connectedAt;
    QDateTime endedAt;
    int statusCode = 0;
    bool missed = false;
    bool seen = false;
};

} // namespace tincan

Q_DECLARE_METATYPE(tincan::HistoryFilter)
Q_DECLARE_METATYPE(tincan::HistoryEntry)
