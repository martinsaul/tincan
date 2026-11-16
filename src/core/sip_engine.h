// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include "core/account_config.h"
#include "core/call_info.h"
#include "core/engine_config.h"

#include <QList>
#include <QString>

namespace tincan {

class ISipEngine {
public:
    virtual ~ISipEngine() = default;
    virtual bool start(const EngineConfig &config, QString *error) = 0;
    virtual void shutdown() = 0;
    virtual int localUdpPort() const = 0;
    virtual bool addAccount(const AccountConfig &config, QString *error) = 0;
    virtual void removeAccount(const QString &accountId) = 0;
    virtual int makeCall(const QString &accountId, const QString &uri, QString *error) = 0;
    virtual bool answer(int callId) = 0;
    virtual bool reject(int callId, int statusCode) = 0;
    virtual bool hangup(int callId) = 0;
    virtual QList<CallInfo> calls() const = 0;
};

} // namespace tincan
