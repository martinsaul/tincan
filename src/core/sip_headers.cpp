// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "core/sip_headers.h"

#include <QStringList>

namespace tincan::SipHeaders {

namespace {

QString canonicalTargetHeader(const QString &rawName) {
    const QString lower = rawName.trimmed().toLower();
    if (lower == QStringLiteral("call-info")) {
        return QStringLiteral("Call-Info");
    }
    if (lower == QStringLiteral("alert-info")) {
        return QStringLiteral("Alert-Info");
    }
    if (lower == QStringLiteral("answer-mode")) {
        return QStringLiteral("Answer-Mode");
    }
    if (lower == QStringLiteral("priority")) {
        return QStringLiteral("Priority");
    }
    return {};
}

QStringList unfoldHeaderLines(const QString &rawMessage) {
    QString normalized = rawMessage;
    normalized.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    normalized.replace(u'\r', u'\n');

    const QStringList lines = normalized.split(u'\n');
    QStringList unfolded;
    bool pastStartLine = false;

    for (const QString &line : lines) {
        if (!pastStartLine) {
            if (line.trimmed().isEmpty()) {
                continue;
            }
            pastStartLine = true;
            if (!line.contains(u':') || line.startsWith(QStringLiteral("SIP/"), Qt::CaseInsensitive) ||
                line.endsWith(QStringLiteral("SIP/2.0"), Qt::CaseInsensitive)) {
                continue;
            }
        }
        if (line.isEmpty()) {
            break;
        }
        if ((line.startsWith(u' ') || line.startsWith(u'\t')) && !unfolded.isEmpty()) {
            unfolded.last().append(u' ');
            unfolded.last().append(line.trimmed());
        } else {
            unfolded.append(line);
        }
    }
    return unfolded;
}

} // namespace

QMap<QString, QString> parseIncomingHeaders(const QString &rawMessage) {
    QMap<QString, QString> result;
    const QStringList headerLines = unfoldHeaderLines(rawMessage);

    for (const QString &line : headerLines) {
        const qsizetype colon = line.indexOf(u':');
        if (colon <= 0) {
            continue;
        }
        const QString canonical = canonicalTargetHeader(line.left(colon));
        if (canonical.isEmpty()) {
            continue;
        }
        const QString value = line.mid(colon + 1).trimmed();
        if (value.isEmpty()) {
            continue;
        }
        auto it = result.find(canonical);
        if (it == result.end()) {
            result.insert(canonical, value);
        } else {
            it.value().append(QStringLiteral(", ") + value);
        }
    }
    return result;
}

} // namespace tincan::SipHeaders
