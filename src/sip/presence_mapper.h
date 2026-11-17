// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include "core/presence_info.h"

#include <pjsua-lib/pjsua.h>
#include <pjsua2/presence.hpp>

#include <QString>

namespace tincan::sip::PresenceMapper {

PresenceInfo fromBuddyInfo(const pj::BuddyInfo &info, const QString &fallbackUri = {});
PresenceInfo fromDialogEventInfo(const pjsua_buddy_dlg_event_info &info,
                                 const QString &fallbackUri = {});
pj::PresenceStatus toPjPresenceStatus(PresenceState state, const QString &note = {});

} // namespace tincan::sip::PresenceMapper
