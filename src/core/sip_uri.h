// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include "core/account_config.h"

#include <QString>

#include <utility>

namespace tincan::SipUri {

QString normalizeDialString(QString dialed);
QString toSipUri(const QString &dialed, const AccountConfig &account);
std::pair<QString, QString> splitNameAddr(const QString &nameAddr);
QString bareSipUri(const QString &uriOrNameAddr);
QString extractUserPart(const QString &uriOrNumber);
QString normalizeContactKey(const QString &numberOrUri);

} // namespace tincan::SipUri
