// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include <QString>

namespace tincan {

struct AudioDevice {
    int index = -1;
    QString name;
    QString driver;
    int inputs = 0;
    int outputs = 0;
};

} // namespace tincan
