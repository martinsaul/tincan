// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include <QString>

namespace tincan {

class Version {
public:
    static QString string();
    // Public source repository (AGPL section 13 source offer).
    static QString sourceUrl();
};

} // namespace tincan
