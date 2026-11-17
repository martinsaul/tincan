// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include <QString>

namespace tincan {

enum class Transport { Udp, Tcp, Tls };

enum class SrtpMode { Disabled, Optional, Mandatory };

struct AccountConfig {
    QString id;
    QString displayName;
    QString user;
    QString domain;
    QString authUser;
    QString password;
    QString proxy;
    Transport transport = Transport::Udp;
    SrtpMode srtp = SrtpMode::Disabled;
    bool registerAccount = true;
    int regExpirySec = 300;
    QString stunServer;
    bool ice = false;
    QString publicAddress;
    QString dialPrefix;
    QString voicemailNumber;
    bool publishPresence = false;

    QString idUri() const;
    QString registrarUri() const;
};

} // namespace tincan
