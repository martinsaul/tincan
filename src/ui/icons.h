// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include "core/registration_info.h"

#include <QColor>
#include <QIcon>
#include <QPixmap>

namespace tincan::ui::Icons {

void ensureInitialized();

QIcon appIcon();
QIcon callStart();
QIcon callStop();
QIcon microphone();
QIcon speakers();
QIcon settings();
QIcon backspace();

QColor regStateColor(RegState state);
QPixmap statusDotPixmap(RegState state, int diameter = 12);
QIcon trayIconForState(RegState state);

} // namespace tincan::ui::Icons
