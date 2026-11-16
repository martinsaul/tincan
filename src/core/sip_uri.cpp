// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "core/sip_uri.h"

namespace tincan::SipUri {

namespace {

bool isAllowedUserChar(QChar ch) {
    const ushort u = ch.unicode();
    const bool isDigit = (u >= u'0' && u <= u'9');
    const bool isUpper = (u >= u'A' && u <= u'Z');
    const bool isLower = (u >= u'a' && u <= u'z');
    return isDigit || isUpper || isLower || ch == u'*' || ch == u'#' || ch == u'_';
}

bool hasSipScheme(const QString &value) {
    return value.startsWith(QStringLiteral("sip:"), Qt::CaseInsensitive) ||
           value.startsWith(QStringLiteral("sips:"), Qt::CaseInsensitive);
}

} // namespace

QString normalizeDialString(QString dialed) {
    const QString trimmed = dialed.trimmed();
    QString result;
    result.reserve(trimmed.size());

    for (const QChar ch : trimmed) {
        if (ch == u'+' && result.isEmpty()) {
            result.append(ch);
        } else if (isAllowedUserChar(ch)) {
            result.append(ch);
        }
    }
    return result;
}

QString toSipUri(const QString &dialed, const AccountConfig &account) {
    const QString trimmed = dialed.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }

    if (hasSipScheme(trimmed)) {
        return trimmed;
    }

    if (trimmed.contains(u'@')) {
        return QStringLiteral("sip:") + trimmed;
    }

    const QString normalized = normalizeDialString(trimmed);
    const QString domain = account.domain.trimmed();
    if (normalized.isEmpty() || domain.isEmpty()) {
        return {};
    }

    return QStringLiteral("sip:%1%2@%3").arg(account.dialPrefix.trimmed(), normalized, domain);
}

} // namespace tincan::SipUri
