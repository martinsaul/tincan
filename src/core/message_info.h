// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include <QDateTime>
#include <QMetaType>
#include <QString>

namespace tincan {

struct MessageInfo {
    QString accountId;
    QString fromUri;
    QString fromName;
    QString body;
    QString contentType;
    QDateTime receivedAt;
};

} // namespace tincan

Q_DECLARE_METATYPE(tincan::MessageInfo)
