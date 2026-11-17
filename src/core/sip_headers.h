// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include <QMap>
#include <QString>

namespace tincan::SipHeaders {

// Parses an incoming SIP request message and extracts only Call-Info,
// Alert-Info, Answer-Mode, and Priority (case-insensitive, RFC 3261 header
// folding and whitespace supported), keyed by canonical header name.
QMap<QString, QString> parseIncomingHeaders(const QString &rawMessage);

} // namespace tincan::SipHeaders
