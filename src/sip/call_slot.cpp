// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "sip/call_slot.h"

namespace tincan::sip {

namespace {

// pj::Call::id is private and pjsua2 offers no way to reset it. Names used in
// an explicit template instantiation are exempt from access checking (C++20
// [temp.spec.general]/6), so instantiating this template with &pj::Call::id
// is the portable way to obtain a pointer to that member without patching
// PJSIP. The friend function defined by the instantiation hands it out.
struct CallIdTag {
    using MemberPtr = pjsua_call_id pj::Call::*;
    friend MemberPtr callIdMember(CallIdTag);
};

template <typename Tag, typename Tag::MemberPtr Member> struct MemberExporter {
    friend typename Tag::MemberPtr callIdMember(Tag) { return Member; }
};

template struct MemberExporter<CallIdTag, &pj::Call::id>;

} // namespace

void forgetCallSlot(pj::Call &call) {
    call.*callIdMember(CallIdTag{}) = PJSUA_INVALID_ID;
}

} // namespace tincan::sip
