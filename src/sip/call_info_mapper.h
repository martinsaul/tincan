// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include "core/call_info.h"

#include <QDateTime>
#include <QString>

#include <pjsua2/call.hpp>

namespace tincan::sip {

class CallInfoMapper {
public:
    CallInfoMapper() = delete;

    static CallInfo map(const pj::CallInfo &pjInfo, const QString &accountId,
                        const QDateTime &connectedAt);
};

} // namespace tincan::sip
