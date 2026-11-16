// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "sip/sip_endpoint.h"

#include "core/version.h"
#include "sip/sip_log_writer.h"

#include <algorithm>
#include <string>

namespace tincan::sip {

namespace {

int parsePortFromSocketAddress(const std::string &address) {
    const std::size_t colonPos = address.rfind(':');
    if (colonPos == std::string::npos || colonPos + 1 >= address.size()) {
        return 0;
    }
    try {
        return std::stoi(address.substr(colonPos + 1));
    } catch (...) {
        return 0;
    }
}

pj::EpConfig buildEpConfig(const EngineConfig &config) {
    pj::EpConfig epCfg;
    epCfg.uaConfig.threadCnt = 1;
    epCfg.uaConfig.mainThreadOnly = false;
    // Round-robin call ids over the largest pool, so an id is rarely reused
    // while its old call's removal is still queued to the main thread.
    epCfg.uaConfig.maxCalls = PJSUA_MAX_CALLS;
    const QString ua = config.userAgent.trimmed().isEmpty()
                           ? QStringLiteral("Tincan/") + Version::string()
                           : config.userAgent.trimmed();
    epCfg.uaConfig.userAgent = ua.toStdString();

    const auto level = static_cast<unsigned>(std::max(0, config.logLevel));
    epCfg.logConfig.level = level;
    epCfg.logConfig.consoleLevel = level;
    auto writer = std::make_unique<SipLogWriter>(config.logFile);
    epCfg.logConfig.writer = writer.release();
    return epCfg;
}

} // namespace

SipEndpoint::~SipEndpoint() {
    shutdown();
}

bool SipEndpoint::start(const EngineConfig &config, QString *error) {
    if (started_) {
        if (error != nullptr) {
            *error = QStringLiteral("SIP endpoint is already started");
        }
        return false;
    }

    try {
        endpoint_ = std::make_unique<pj::Endpoint>();
        endpoint_->libCreate();
        endpoint_->libInit(buildEpConfig(config));
        createTransports(config);
        endpoint_->libStart();
        if (config.nullAudio) {
            endpoint_->audDevManager().setNullDev();
        }
        started_ = true;
        return true;
    } catch (const pj::Error &err) {
        if (error != nullptr) {
            *error = QString::fromStdString(err.info());
        }
        shutdown();
        return false;
    }
}

void SipEndpoint::createTransports(const EngineConfig &config) {
    pj::TransportConfig udpCfg;
    udpCfg.port = static_cast<unsigned>(std::max(0, config.udpPort));
    const pj::TransportId udpId = endpoint_->transportCreate(PJSIP_TRANSPORT_UDP, udpCfg);
    const pj::TransportInfo udpInfo = endpoint_->transportGetInfo(udpId);
    localUdpPort_ = parsePortFromSocketAddress(udpInfo.localAddress);
    if (localUdpPort_ == 0) {
        localUdpPort_ = parsePortFromSocketAddress(udpInfo.localName);
    }

    if (config.enableTcp) {
        pj::TransportConfig tcpCfg;
        tcpCfg.port = static_cast<unsigned>(std::max(0, config.tcpPort));
        (void)endpoint_->transportCreate(PJSIP_TRANSPORT_TCP, tcpCfg);
    }

    if (config.enableTls) {
        pj::TransportConfig tlsCfg;
        tlsCfg.port = static_cast<unsigned>(std::max(0, config.tlsPort));
        (void)endpoint_->transportCreate(PJSIP_TRANSPORT_TLS, tlsCfg);
    }
}

void SipEndpoint::shutdown() {
    if (endpoint_) {
        try {
            endpoint_->libDestroy();
        } catch (const pj::Error &) {
        }
        endpoint_.reset();
    }
    localUdpPort_ = 0;
    started_ = false;
}

void SipEndpoint::hangupAllCalls() {
    if (endpoint_) {
        endpoint_->hangupAllCalls();
    }
}

int SipEndpoint::handleEvents(unsigned timeoutMs) {
    return endpoint_ ? endpoint_->libHandleEvents(timeoutMs) : 0;
}

void SipEndpoint::stopWorkerThreads() {
    if (endpoint_) {
        endpoint_->libStopWorkerThreads();
    }
}

void SipEndpoint::updateStunServer(const QString &stunServer) {
    if (!endpoint_) {
        return;
    }
    pj::StringVector servers;
    const QString trimmed = stunServer.trimmed();
    if (!trimmed.isEmpty()) {
        servers.push_back(trimmed.toStdString());
    }
    endpoint_->natUpdateStunServers(servers, false);
}

int SipEndpoint::localUdpPort() const {
    return localUdpPort_;
}

bool SipEndpoint::isStarted() const {
    return started_;
}

pj::Endpoint &SipEndpoint::pjEndpoint() {
    return *endpoint_;
}

} // namespace tincan::sip
