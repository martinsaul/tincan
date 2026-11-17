// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include <QDateTime>
#include <QMetaType>
#include <QString>

namespace tincan {

struct Contact {
    qint64 id = 0;
    QString name;
    QString number;
    QString accountId;
    bool presence = false;
    QString notes;
    QDateTime createdAt;
    QDateTime updatedAt;
};

} // namespace tincan

Q_DECLARE_METATYPE(tincan::Contact)
