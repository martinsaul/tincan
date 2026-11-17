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

QString stripSipScheme(const QString &value) {
    if (value.startsWith(QStringLiteral("sips:"), Qt::CaseInsensitive)) {
        return value.mid(5);
    }
    if (value.startsWith(QStringLiteral("sip:"), Qt::CaseInsensitive)) {
        return value.mid(4);
    }
    return value;
}

QString stripUriParamsAndHeaders(const QString &uri) {
    qsizetype cut = uri.indexOf(u';');
    const qsizetype qIdx = uri.indexOf(u'?');
    if (qIdx >= 0 && (cut < 0 || qIdx < cut)) {
        cut = qIdx;
    }
    return cut >= 0 ? uri.left(cut).trimmed() : uri.trimmed();
}

QString unquoteDisplayName(const QString &raw) {
    QString trimmed = raw.trimmed();
    if (trimmed.size() >= 2 && trimmed.startsWith(u'"') && trimmed.endsWith(u'"')) {
        trimmed = trimmed.mid(1, trimmed.size() - 2);
    }
    QString unescaped;
    unescaped.reserve(trimmed.size());
    for (qsizetype i = 0; i < trimmed.size(); ++i) {
        if (trimmed[i] == u'\\' && i + 1 < trimmed.size()) {
            ++i;
        }
        unescaped.append(trimmed[i]);
    }
    return unescaped;
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
    const auto [extracted, unusedDisplay] = splitNameAddr(dialed);
    (void)unusedDisplay;
    const QString trimmed = extracted.trimmed();
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

std::pair<QString, QString> splitNameAddr(const QString &nameAddr) {
    const QString trimmed = nameAddr.trimmed();
    const qsizetype openBracket = trimmed.indexOf(u'<');
    const qsizetype closeBracket = trimmed.lastIndexOf(u'>');
    if (openBracket >= 0 && closeBracket > openBracket) {
        const QString display = unquoteDisplayName(trimmed.left(openBracket));
        const QString uri = trimmed.mid(openBracket + 1, closeBracket - openBracket - 1).trimmed();
        return {uri, display};
    }
    return {trimmed, QString{}};
}

QString bareSipUri(const QString &uriOrNameAddr) {
    const auto [uri, unusedDisplay] = splitNameAddr(uriOrNameAddr);
    (void)unusedDisplay;
    const QString clean = stripUriParamsAndHeaders(uri);
    if (clean.isEmpty()) {
        return {};
    }
    if (clean.startsWith(QStringLiteral("sips:"), Qt::CaseInsensitive)) {
        return QStringLiteral("sips:") + clean.mid(5).trimmed();
    }
    if (clean.startsWith(QStringLiteral("sip:"), Qt::CaseInsensitive)) {
        return QStringLiteral("sip:") + clean.mid(4).trimmed();
    }
    if (clean.contains(u'@') || clean.contains(u':')) {
        return QStringLiteral("sip:") + clean;
    }
    return clean;
}

QString extractUserPart(const QString &uriOrNumber) {
    const auto [uri, unusedDisplay] = splitNameAddr(uriOrNumber);
    (void)unusedDisplay;
    const QString withoutParams = stripUriParamsAndHeaders(uri);
    const QString body = stripSipScheme(withoutParams);
    const qsizetype atIdx = body.indexOf(u'@');
    if (atIdx >= 0) {
        return body.left(atIdx).trimmed();
    }
    if (hasSipScheme(withoutParams) || body.contains(u':') || body.contains(u'.')) {
        return {};
    }
    return body.trimmed();
}

QString normalizeContactKey(const QString &numberOrUri) {
    const auto [uri, unusedDisplay] = splitNameAddr(numberOrUri);
    (void)unusedDisplay;
    const QString clean = stripUriParamsAndHeaders(uri);
    if (clean.isEmpty()) {
        return {};
    }
    if (hasSipScheme(clean) || clean.contains(u'@') || clean.contains(u'.')) {
        return stripSipScheme(clean).toLower();
    }
    const QString normalized = normalizeDialString(clean);
    return normalized.isEmpty() ? clean.toLower() : normalized.toLower();
}

} // namespace tincan::SipUri
