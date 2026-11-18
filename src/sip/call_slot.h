// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include <pjsua2/call.hpp>

namespace tincan::sip {

// Makes `call` forget its pjsua call id, so that pj::Call's destructor
// neither clears the slot's user data nor hangs the slot up.
//
// Use only once the call's DISCONNECTED state has been processed. pjsua2 has
// then already removed the call's media and its user-data association, but it
// keeps the old id, and pjsua is free to hand that id to a new call. Without
// this, deleting the stale object would detach (and possibly hang up) the
// unrelated new call that reused the slot.
void forgetCallSlot(pj::Call &call);

} // namespace tincan::sip
