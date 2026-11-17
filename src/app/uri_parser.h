// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include <QString>

namespace tincan::UriParser {

QString parse(const QString &uri);
bool hasSupportedScheme(const QString &text);

} // namespace tincan::UriParser
