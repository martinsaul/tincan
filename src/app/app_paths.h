// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include <QString>

namespace tincan {

class AppPaths {
public:
    static QString configDir();
    static QString dataDir();
};

} // namespace tincan
