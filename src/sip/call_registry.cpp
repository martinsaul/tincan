// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "sip/call_registry.h"

#include <QtGlobal>

#include <utility>

namespace tincan::sip {

void CallRegistry::insert(int callId, std::unique_ptr<SipCall> call) {
    if (callId < 0 || call == nullptr) {
        return;
    }
    auto &slot = calls_[callId];
    if (slot != nullptr) {
        // A disconnected call whose removal is still queued. Deleting it now
        // would clear PJSIP's user data for the reused id, so leak it instead.
        qWarning("tincan: call id %d reused before its old call was released", callId);
        (void)slot.release();
    }
    slot = std::move(call);
}

SipCall *CallRegistry::find(int callId) const {
    const auto it = calls_.find(callId);
    return it != calls_.end() ? it->second.get() : nullptr;
}

void CallRegistry::erase(int callId) {
    calls_.erase(callId);
}

void CallRegistry::eraseIfSame(int callId, const SipCall *call) {
    const auto it = calls_.find(callId);
    if (it != calls_.end() && it->second.get() == call) {
        calls_.erase(it);
    }
}

void CallRegistry::clear() {
    calls_.clear();
}

bool CallRegistry::isEmpty() const {
    return calls_.empty();
}

QList<CallInfo> CallRegistry::snapshots() const {
    QList<CallInfo> result;
    result.reserve(static_cast<qsizetype>(calls_.size()));
    for (const auto &[id, call] : calls_) {
        (void)id;
        try {
            result.push_back(call->snapshot());
        } catch (const pj::Error &) {
        }
    }
    return result;
}

} // namespace tincan::sip
