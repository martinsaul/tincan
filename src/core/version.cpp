// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "core/version.h"

#include "core/version_config.h"

namespace tincan {

QString Version::string() {
    return QStringLiteral(TINCAN_VERSION_STRING);
}

QString Version::sourceUrl() {
    return QStringLiteral(TINCAN_SOURCE_URL);
}

} // namespace tincan
