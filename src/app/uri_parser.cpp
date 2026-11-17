// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "app/uri_parser.h"

#include <QByteArray>
#include <QStringDecoder>
#include <QStringList>
#include <QStringView>

namespace tincan::UriParser {

namespace {

int hexValue(char ch) {
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    if (ch >= 'A' && ch <= 'F') {
        return 10 + (ch - 'A');
    }
    if (ch >= 'a' && ch <= 'f') {
        return 10 + (ch - 'a');
    }
    return -1;
}

bool containsControlChar(QStringView text) {
    for (const QChar ch : text) {
        if (ch.unicode() <= 0x1F || ch.unicode() == 0x7F) {
            return true;
        }
    }
    return false;
}

bool containsWhitespace(QStringView text) {
    for (const QChar ch : text) {
        if (ch.isSpace()) {
            return true;
        }
    }
    return false;
}

QString decodePercentStrict(QStringView input, bool *ok) {
    *ok = false;
    if (containsControlChar(input)) {
        return {};
    }
    const QByteArray utf8 = input.toUtf8();
    QByteArray decodedBytes;
    decodedBytes.reserve(utf8.size());
    for (qsizetype i = 0; i < utf8.size(); ++i) {
        if (utf8[i] == '%') {
            if (i + 2 >= utf8.size()) {
                return {};
            }
            const int hi = hexValue(utf8[i + 1]);
            const int lo = hexValue(utf8[i + 2]);
            if (hi < 0 || lo < 0) {
                return {};
            }
            decodedBytes.append(static_cast<char>((hi << 4) | lo));
            i += 2;
        } else {
            decodedBytes.append(utf8[i]);
        }
    }
    QStringDecoder decoder(QStringDecoder::Utf8, QStringDecoder::Flag::Stateless);
    const QString result = decoder.decode(decodedBytes);
    if (decoder.hasError() || containsControlChar(result)) {
        return {};
    }
    *ok = true;
    return result;
}

QString normalizePhoneDigits(QStringView input) {
    const QStringView trimmed = input.trimmed();
    QString out;
    out.reserve(trimmed.size());
    bool hasDigit = false;
    for (qsizetype i = 0; i < trimmed.size(); ++i) {
        const QChar ch = trimmed[i];
        if (ch == u'+' && i == 0) {
            out.append(ch);
        } else if ((ch >= u'0' && ch <= u'9') || ch == u'*' || ch == u'#') {
            out.append(ch);
            hasDigit = true;
        } else if (ch == u'-' || ch == u'.' || ch == u'(' || ch == u')' || ch == u' ') {
            continue;
        } else {
            return {};
        }
    }
    return hasDigit ? out : QString{};
}

bool isValidPort(QStringView portView) {
    if (portView.isEmpty() || portView.size() > 5) {
        return false;
    }
    int value = 0;
    for (const QChar ch : portView) {
        if (ch < u'0' || ch > u'9') {
            return false;
        }
        value = value * 10 + (ch.unicode() - u'0');
    }
    return value >= 1 && value <= 65535;
}

bool isValidHostname(QStringView host) {
    if (host.isEmpty() || host.startsWith(u'.') || host.endsWith(u'.') || host.startsWith(u'-') ||
        host.endsWith(u'-') || host.contains(QStringLiteral(".."))) {
        return false;
    }
    for (const QChar ch : host) {
        const ushort u = ch.unicode();
        const bool alphaNum =
            (u >= u'0' && u <= u'9') || (u >= u'A' && u <= u'Z') || (u >= u'a' && u <= u'z');
        if (!alphaNum && ch != u'.' && ch != u'-') {
            return false;
        }
    }
    return true;
}

bool isValidIpv6HostPort(QStringView hostPort) {
    const qsizetype closeBracket = hostPort.indexOf(u']');
    if (closeBracket <= 1) {
        return false;
    }
    const QStringView inside = hostPort.mid(1, closeBracket - 1);
    for (const QChar ch : inside) {
        const ushort u = ch.unicode();
        const bool hex = (u >= u'0' && u <= u'9') || (u >= u'A' && u <= u'F') ||
                         (u >= u'a' && u <= u'f');
        if (!hex && ch != u':' && ch != u'.') {
            return false;
        }
    }
    const QStringView tail = hostPort.mid(closeBracket + 1);
    return tail.isEmpty() || (tail.startsWith(u':') && isValidPort(tail.mid(1)));
}

bool isValidHostPort(QStringView hostPort) {
    if (hostPort.isEmpty()) {
        return false;
    }
    if (hostPort.startsWith(u'[')) {
        return isValidIpv6HostPort(hostPort);
    }
    const qsizetype colon = hostPort.indexOf(u':');
    if (colon < 0) {
        return isValidHostname(hostPort);
    }
    return isValidHostname(hostPort.left(colon)) && isValidPort(hostPort.mid(colon + 1));
}

bool isValidSipUserChar(QChar ch) {
    const ushort u = ch.unicode();
    if ((u >= u'0' && u <= u'9') || (u >= u'A' && u <= u'Z') || (u >= u'a' && u <= u'z')) {
        return true;
    }
    const QStringView extra = u"-_.!~*'()&=+$,;:#";
    return extra.contains(ch);
}

bool isValidSipUserInfo(QStringView user) {
    if (user.isEmpty()) {
        return false;
    }
    for (const QChar ch : user) {
        if (!isValidSipUserChar(ch)) {
            return false;
        }
    }
    return true;
}

bool isValidSipParamChar(QChar ch) {
    const ushort u = ch.unicode();
    if ((u >= u'0' && u <= u'9') || (u >= u'A' && u <= u'Z') || (u >= u'a' && u <= u'z')) {
        return true;
    }
    const QStringView extra = u"-_.!~*'()[]/:&+$=";
    return extra.contains(ch);
}

QString parseTel(QStringView rest) {
    qsizetype cut = rest.indexOf(u';');
    const qsizetype qIdx = rest.indexOf(u'?');
    if (qIdx >= 0 && (cut < 0 || qIdx < cut)) {
        cut = qIdx;
    }
    const QStringView subscriber = cut >= 0 ? rest.left(cut) : rest;
    bool ok = false;
    const QString decoded = decodePercentStrict(subscriber, &ok);
    return ok ? normalizePhoneDigits(decoded) : QString{};
}

bool isValidCalltoUser(QStringView user) {
    if (user.isEmpty()) {
        return false;
    }
    for (qsizetype i = 0; i < user.size(); ++i) {
        const QChar ch = user[i];
        const ushort u = ch.unicode();
        const bool alphaNum =
            (u >= u'0' && u <= u'9') || (u >= u'A' && u <= u'Z') || (u >= u'a' && u <= u'z');
        const bool extra = ch == u'_' || ch == u'-' || ch == u'.' || ch == u'*' || ch == u'#' ||
                           (ch == u'+' && i == 0);
        if (!alphaNum && !extra) {
            return false;
        }
    }
    return true;
}

QString parseCallto(QStringView rest) {
    if (rest.startsWith(u"//")) {
        rest = rest.mid(2);
    }
    const qsizetype qIdx = rest.indexOf(u'?');
    if (qIdx >= 0) {
        rest = rest.left(qIdx);
    }
    if (rest.endsWith(u'/')) {
        rest.chop(1);
    }
    if (rest.isEmpty() || containsWhitespace(rest) || rest.contains(u'/')) {
        return {};
    }
    bool ok = false;
    const QString decoded = decodePercentStrict(rest, &ok);
    if (!ok || decoded.isEmpty()) {
        return {};
    }
    const QString asPhone = normalizePhoneDigits(decoded);
    if (!asPhone.isEmpty()) {
        return asPhone;
    }
    const qsizetype atIdx = decoded.indexOf(u'@');
    if (atIdx >= 0) {
        const QStringView user = QStringView{decoded}.left(atIdx);
        const QStringView host = QStringView{decoded}.mid(atIdx + 1);
        return (!host.contains(u'@') && isValidCalltoUser(user) && isValidHostPort(host))
                   ? decoded
                   : QString{};
    }
    return isValidCalltoUser(decoded) ? decoded : QString{};
}

QString decodeSipParams(QStringView rawParams, bool *ok) {
    *ok = true;
    if (rawParams.isEmpty()) {
        return {};
    }
    const QList<QStringView> parts = rawParams.mid(1).split(u';');
    QString out;
    for (const QStringView part : parts) {
        bool partOk = false;
        const QString decoded = decodePercentStrict(part, &partOk);
        if (!partOk || decoded.isEmpty()) {
            *ok = false;
            return {};
        }
        for (const QChar ch : decoded) {
            if (!isValidSipParamChar(ch)) {
                *ok = false;
                return {};
            }
        }
        out.append(u';');
        out.append(decoded);
    }
    return out;
}

QString parseSip(QStringView normalizedScheme, QStringView rest) {
    if (rest.isEmpty() || containsWhitespace(rest) || rest.contains(u'<') || rest.contains(u'>') ||
        rest.contains(u'"') || rest.contains(u'\\')) {
        return {};
    }
    const qsizetype qIdx = rest.indexOf(u'?');
    if (qIdx >= 0) {
        bool hdrOk = false;
        (void)decodePercentStrict(rest.mid(qIdx + 1), &hdrOk);
        if (!hdrOk) {
            return {};
        }
    }
    const QStringView beforeHeaders = qIdx >= 0 ? rest.left(qIdx) : rest;
    const qsizetype semiIdx = beforeHeaders.indexOf(u';');
    const QStringView rawAddr = semiIdx >= 0 ? beforeHeaders.left(semiIdx) : beforeHeaders;
    const QStringView rawParams = semiIdx >= 0 ? beforeHeaders.mid(semiIdx) : QStringView{};

    bool addrOk = false;
    const QString addr = decodePercentStrict(rawAddr, &addrOk);
    if (!addrOk || addr.isEmpty()) {
        return {};
    }
    const qsizetype atIdx = addr.indexOf(u'@');
    const bool validAddr =
        (atIdx >= 0) ? (!QStringView{addr}.mid(atIdx + 1).contains(u'@') &&
                        isValidSipUserInfo(QStringView{addr}.left(atIdx)) &&
                        isValidHostPort(QStringView{addr}.mid(atIdx + 1)))
                     : isValidHostPort(addr);
    if (!validAddr) {
        return {};
    }

    bool paramsOk = false;
    const QString params = decodeSipParams(rawParams, &paramsOk);
    if (!paramsOk) {
        return {};
    }
    return QString(normalizedScheme) + addr + params;
}

} // namespace

bool hasSupportedScheme(const QString &text) {
    const QString trimmed = text.trimmed();
    return trimmed.startsWith(QStringLiteral("sip:"), Qt::CaseInsensitive) ||
           trimmed.startsWith(QStringLiteral("sips:"), Qt::CaseInsensitive) ||
           trimmed.startsWith(QStringLiteral("tel:"), Qt::CaseInsensitive) ||
           trimmed.startsWith(QStringLiteral("callto:"), Qt::CaseInsensitive);
}

QString parse(const QString &uri) {
    const QString trimmed = uri.trimmed();
    if (trimmed.startsWith(QStringLiteral("tel:"), Qt::CaseInsensitive)) {
        return parseTel(QStringView{trimmed}.mid(4));
    }
    if (trimmed.startsWith(QStringLiteral("callto:"), Qt::CaseInsensitive)) {
        return parseCallto(QStringView{trimmed}.mid(7));
    }
    if (trimmed.startsWith(QStringLiteral("sips:"), Qt::CaseInsensitive)) {
        return parseSip(u"sips:", QStringView{trimmed}.mid(5));
    }
    if (trimmed.startsWith(QStringLiteral("sip:"), Qt::CaseInsensitive)) {
        return parseSip(u"sip:", QStringView{trimmed}.mid(4));
    }
    return {};
}

} // namespace tincan::UriParser
