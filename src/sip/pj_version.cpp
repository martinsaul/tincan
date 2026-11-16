// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "sip/pj_version.h"

#include <pjlib.h>

namespace tincan::sip {

std::string PjVersion::string() {
    const char *ver = pj_get_version();
    return ver != nullptr ? std::string(ver) : std::string();
}

} // namespace tincan::sip
