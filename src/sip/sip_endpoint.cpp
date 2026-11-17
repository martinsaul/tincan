// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "sip/sip_endpoint.h"

#include "core/version.h"
#include "sip/sip_log_writer.h"

#include <algorithm>
#include <cstddef>
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
        nullAudio_ = config.nullAudio;
        if (nullAudio_) {
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
    nullAudio_ = false;
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

QList<AudioDevice> SipEndpoint::audioDevices() const {
    if (!started_ || !endpoint_) {
        return {};
    }
    try {
        const pj::AudioDevInfoVector2 devs = endpoint_->audDevManager().enumDev2();
        QList<AudioDevice> result;
        result.reserve(static_cast<qsizetype>(devs.size()));
        for (std::size_t i = 0; i < devs.size(); ++i) {
            AudioDevice dev;
            dev.index = static_cast<int>(i);
            dev.name = QString::fromStdString(devs[i].name);
            dev.driver = QString::fromStdString(devs[i].driver);
            dev.inputs = static_cast<int>(devs[i].inputCount);
            dev.outputs = static_cast<int>(devs[i].outputCount);
            result.append(dev);
        }
        return result;
    } catch (const pj::Error &) {
        return {};
    }
}

bool SipEndpoint::setAudioDevices(int captureIndex, int playbackIndex) {
    if (!started_ || !endpoint_) {
        return false;
    }
    if (nullAudio_) {
        return true;
    }
    try {
        pj::AudDevManager &mgr = endpoint_->audDevManager();
        const int cap = captureIndex < 0 ? PJMEDIA_AUD_DEFAULT_CAPTURE_DEV : captureIndex;
        const int play = playbackIndex < 0 ? PJMEDIA_AUD_DEFAULT_PLAYBACK_DEV : playbackIndex;
        mgr.setCaptureDev(cap);
        mgr.setPlaybackDev(play);
        return true;
    } catch (const pj::Error &) {
        return false;
    }
}

QList<CodecInfo> SipEndpoint::codecs() const {
    if (!started_ || !endpoint_) {
        return {};
    }
    try {
        const pj::CodecInfoVector2 pjCodecs = endpoint_->codecEnum2();
        QList<CodecInfo> result;
        result.reserve(static_cast<qsizetype>(pjCodecs.size()));
        for (const pj::CodecInfo &c : pjCodecs) {
            result.append(CodecInfo{QString::fromStdString(c.codecId), static_cast<int>(c.priority)});
        }
        return result;
    } catch (const pj::Error &) {
        return {};
    }
}

bool SipEndpoint::setCodecPriority(const QString &id, int priority) {
    if (!started_ || !endpoint_ || id.trimmed().isEmpty() || priority < 0 || priority > 255) {
        return false;
    }
    try {
        endpoint_->codecSetPriority(id.toStdString(), static_cast<pj_uint8_t>(priority));
        return true;
    } catch (const pj::Error &) {
        return false;
    }
}

bool SipEndpoint::setEchoCanceller(bool enabled, int tailMs) {
    if (!started_ || !endpoint_) {
        return false;
    }
    const unsigned tail = (enabled && tailMs > 0) ? static_cast<unsigned>(tailMs) : 0U;
    try {
        endpoint_->audDevManager().setEcOptions(tail, 0);
        return true;
    } catch (const pj::Error &) {
        return false;
    }
}

int SipEndpoint::localUdpPort() const {
    return localUdpPort_;
}

bool SipEndpoint::isStarted() const {
    return started_;
}

bool SipEndpoint::isNullAudio() const {
    return nullAudio_;
}

pj::Endpoint &SipEndpoint::pjEndpoint() {
    return *endpoint_;
}

} // namespace tincan::sip
