// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include "core/call_info.h"
#include "core/presence_info.h"
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

QColor presenceStateColor(PresenceState state);
QPixmap presenceDotPixmap(PresenceState state, int diameter = 10);
QIcon presenceDotIcon(PresenceState state, int diameter = 10);
QIcon historyDirectionIcon(CallDirection direction, bool missed);

} // namespace tincan::ui::Icons
