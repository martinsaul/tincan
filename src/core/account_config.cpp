// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "core/account_config.h"

namespace tincan {

namespace {

QString escapeQuotedDisplayName(const QString &raw) {
    QString escaped;
    escaped.reserve(raw.size());
    for (const QChar ch : raw) {
        if (ch == u'\\' || ch == u'"') {
            escaped.append(u'\\');
        }
        escaped.append(ch);
    }
    return escaped;
}

QString stripSipScheme(const QString &value) {
    if (value.startsWith(QStringLiteral("sips:"), Qt::CaseInsensitive)) {
        return value.mid(5);
    }
    if (value.startsWith(QStringLiteral("sip:"), Qt::CaseInsensitive)) {
        return value.mid(4);
    }
    return value;
}

} // namespace

QString AccountConfig::idUri() const {
    const QString host = stripSipScheme(domain.trimmed());
    if (host.isEmpty()) {
        return {};
    }

    const QString trimmedUser = user.trimmed();
    const QString coreUri = trimmedUser.isEmpty()
                                ? QStringLiteral("sip:%1").arg(host)
                                : QStringLiteral("sip:%1@%2").arg(trimmedUser, host);

    const QString trimmedDisplay = displayName.trimmed();
    if (trimmedDisplay.isEmpty()) {
        return coreUri;
    }

    return QStringLiteral("\"%1\" <%2>").arg(escapeQuotedDisplayName(trimmedDisplay), coreUri);
}

QString AccountConfig::registrarUri() const {
    const QString host = stripSipScheme(domain.trimmed());
    if (!registerAccount || host.isEmpty()) {
        return {};
    }

    QString uri = QStringLiteral("sip:%1").arg(host);
    if (transport == Transport::Tcp) {
        uri.append(QStringLiteral(";transport=tcp"));
    } else if (transport == Transport::Tls) {
        uri.append(QStringLiteral(";transport=tls"));
    }
    return uri;
}

} // namespace tincan
