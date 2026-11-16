// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include "core/account_config.h"

#include <QString>

namespace tincan::SipUri {

QString normalizeDialString(QString dialed);
QString toSipUri(const QString &dialed, const AccountConfig &account);

} // namespace tincan::SipUri
