// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include "core/engine_config.h"

#include <QString>

#include <pjsua2/endpoint.hpp>

#include <memory>

namespace tincan::sip {

class SipEndpoint {
public:
    SipEndpoint() = default;
    ~SipEndpoint();

    SipEndpoint(const SipEndpoint &) = delete;
    SipEndpoint &operator=(const SipEndpoint &) = delete;

    bool start(const EngineConfig &config, QString *error);
    void shutdown();

    void hangupAllCalls();
    int handleEvents(unsigned timeoutMs);
    void stopWorkerThreads();
    void updateStunServer(const QString &stunServer);

    int localUdpPort() const;
    bool isStarted() const;
    pj::Endpoint &pjEndpoint();

private:
    void createTransports(const EngineConfig &config);

    std::unique_ptr<pj::Endpoint> endpoint_;
    int localUdpPort_ = 0;
    bool started_ = false;
};

} // namespace tincan::sip
