// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include <QString>

namespace tincan {

struct EngineConfig {
    int udpPort = 5060;
    int tcpPort = 5060;
    int tlsPort = 5061;
    bool enableTcp = true;
    bool enableTls = false;
    bool nullAudio = false;
    QString userAgent;
    int logLevel = 3;
    QString logFile;
};

} // namespace tincan
