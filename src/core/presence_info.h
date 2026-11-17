// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include <QMetaType>
#include <QString>

namespace tincan {

enum class PresenceState {
    Unknown,
    Offline,
    Online,
    Away,
    Busy,
    Ringing,
    OnThePhone,
};

struct PresenceInfo {
    QString uri;
    PresenceState state = PresenceState::Unknown;
    QString note;
};

} // namespace tincan

Q_DECLARE_METATYPE(tincan::PresenceState)
Q_DECLARE_METATYPE(tincan::PresenceInfo)
