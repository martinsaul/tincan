// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "sip/call_info_mapper.h"

#include <utility>

namespace tincan::sip {

namespace {

CallState mapCallState(pjsip_inv_state state, pjsip_role_e role) {
    switch (state) {
    case PJSIP_INV_STATE_NULL:
        return role == PJSIP_ROLE_UAS ? CallState::Incoming : CallState::Calling;
    case PJSIP_INV_STATE_CALLING:
        return CallState::Calling;
    case PJSIP_INV_STATE_INCOMING:
        return CallState::Incoming;
    case PJSIP_INV_STATE_EARLY:
        return CallState::Early;
    case PJSIP_INV_STATE_CONNECTING:
        return CallState::Connecting;
    case PJSIP_INV_STATE_CONFIRMED:
        return CallState::Confirmed;
    case PJSIP_INV_STATE_DISCONNECTED:
        return CallState::Disconnected;
    }
    return CallState::Disconnected;
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

std::pair<QString, QString> splitNameAddr(const QString &remoteInfo) {
    const QString trimmed = remoteInfo.trimmed();
    const qsizetype openBracket = trimmed.indexOf(u'<');
    const qsizetype closeBracket = trimmed.lastIndexOf(u'>');
    if (openBracket >= 0 && closeBracket > openBracket) {
        const QString display = unquoteDisplayName(trimmed.left(openBracket));
        const QString uri = trimmed.mid(openBracket + 1, closeBracket - openBracket - 1).trimmed();
        return {uri, display};
    }
    return {trimmed, QString{}};
}

} // namespace

CallInfo CallInfoMapper::map(const pj::CallInfo &pjInfo, const QString &accountId,
                             const QDateTime &connectedAt) {
    const auto [remoteUri, remoteDisplay] = splitNameAddr(QString::fromStdString(pjInfo.remoteUri));

    CallInfo info;
    info.callId = pjInfo.id;
    info.accountId = accountId;
    info.direction =
        pjInfo.role == PJSIP_ROLE_UAS ? CallDirection::Incoming : CallDirection::Outgoing;
    info.state = mapCallState(pjInfo.state, pjInfo.role);
    info.remoteUri = remoteUri;
    info.remoteDisplayName = remoteDisplay;
    info.lastStatusCode = static_cast<int>(pjInfo.lastStatusCode);
    info.lastReason = QString::fromStdString(pjInfo.lastReason);
    info.connectedAt = connectedAt;
    return info;
}

} // namespace tincan::sip
