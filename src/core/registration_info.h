// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include <QString>

namespace tincan {

enum class RegState { Unregistered, Registering, Registered, Failed };

struct RegistrationInfo {
    QString accountId;
    RegState state = RegState::Unregistered;
    int statusCode = 0;
    QString reason;
};

} // namespace tincan
