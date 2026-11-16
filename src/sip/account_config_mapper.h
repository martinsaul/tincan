// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include "core/account_config.h"

#include <pjsua2/account.hpp>

namespace tincan::sip {

class AccountConfigMapper {
public:
    AccountConfigMapper() = delete;

    static pj::AccountConfig map(const AccountConfig &config);
};

} // namespace tincan::sip
