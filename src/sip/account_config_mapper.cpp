// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "sip/account_config_mapper.h"

namespace tincan::sip {

namespace {

pjmedia_srtp_use mapSrtpMode(SrtpMode mode) {
    switch (mode) {
    case SrtpMode::Disabled:
        return PJMEDIA_SRTP_DISABLED;
    case SrtpMode::Optional:
        return PJMEDIA_SRTP_OPTIONAL;
    case SrtpMode::Mandatory:
        return PJMEDIA_SRTP_MANDATORY;
    }
    return PJMEDIA_SRTP_DISABLED;
}

QString buildFallbackDirectIpUri(const AccountConfig &config) {
    AccountConfig fallback = config;
    fallback.domain = QStringLiteral("127.0.0.1");
    return fallback.idUri();
}

QString normalizeProxyUri(const QString &proxy) {
    const QString trimmed = proxy.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }
    if (trimmed.startsWith(QStringLiteral("sip:"), Qt::CaseInsensitive) ||
        trimmed.startsWith(QStringLiteral("sips:"), Qt::CaseInsensitive)) {
        return trimmed;
    }
    return QStringLiteral("sip:") + trimmed;
}

void mapRegistration(const AccountConfig &src, pj::AccountConfig &dst) {
    const unsigned expiry = src.regExpirySec > 0 ? static_cast<unsigned>(src.regExpirySec) : 300u;
    dst.regConfig.timeoutSec = expiry;

    if (src.registerAccount && !src.domain.trimmed().isEmpty()) {
        dst.regConfig.registrarUri = src.registrarUri().toStdString();
        dst.regConfig.registerOnAdd = true;
    } else {
        dst.regConfig.registrarUri.clear();
        dst.regConfig.registerOnAdd = false;
    }
}

void mapSipAuthAndProxy(const AccountConfig &src, pj::AccountConfig &dst) {
    const QString effectiveUser =
        src.authUser.trimmed().isEmpty() ? src.user.trimmed() : src.authUser.trimmed();
    if (!effectiveUser.isEmpty() && (!src.password.isEmpty() || !src.authUser.trimmed().isEmpty())) {
        dst.sipConfig.authCreds.emplace_back("digest", "*", effectiveUser.toStdString(), 0,
                                             src.password.toStdString());
    }

    const QString proxyUri = normalizeProxyUri(src.proxy);
    if (!proxyUri.isEmpty()) {
        dst.sipConfig.proxies.push_back(proxyUri.toStdString());
    }
}

} // namespace

pj::AccountConfig AccountConfigMapper::map(const AccountConfig &config) {
    pj::AccountConfig pjCfg;

    const QString idUri = config.idUri().isEmpty() ? buildFallbackDirectIpUri(config) : config.idUri();
    pjCfg.idUri = idUri.toStdString();

    mapRegistration(config, pjCfg);
    mapSipAuthAndProxy(config, pjCfg);

    pjCfg.natConfig.iceEnabled = config.ice;
    const bool hasStun = !config.stunServer.trimmed().isEmpty();
    pjCfg.natConfig.sipStunUse = hasStun ? PJSUA_STUN_USE_DEFAULT : PJSUA_STUN_USE_DISABLED;
    pjCfg.natConfig.mediaStunUse = hasStun ? PJSUA_STUN_USE_DEFAULT : PJSUA_STUN_USE_DISABLED;

    pjCfg.mediaConfig.srtpUse = mapSrtpMode(config.srtp);
    pjCfg.mediaConfig.srtpSecureSignaling = 0;
    if (!config.publicAddress.trimmed().isEmpty()) {
        pjCfg.mediaConfig.transportConfig.publicAddress = config.publicAddress.trimmed().toStdString();
    }

    return pjCfg;
}

} // namespace tincan::sip
