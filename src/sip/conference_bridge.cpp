// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "sip/conference_bridge.h"

#include "sip/call_registry.h"
#include "sip/sip_call.h"

#include <vector>

namespace tincan::sip {

namespace {

void connectPairBidirectional(SipCall &first, SipCall &second) {
    pj::AudioMedia audioA;
    pj::AudioMedia audioB;
    if (!first.tryGetAudioMedia(&audioA) || !second.tryGetAudioMedia(&audioB)) {
        return;
    }
    try {
        audioA.startTransmit(audioB);
        audioB.startTransmit(audioA);
    } catch (const pj::Error &) {
    }
}

void disconnectPairBidirectional(SipCall &first, SipCall &second) {
    pj::AudioMedia audioA;
    pj::AudioMedia audioB;
    if (!first.tryGetAudioMedia(&audioA) || !second.tryGetAudioMedia(&audioB)) {
        return;
    }
    try {
        audioA.stopTransmit(audioB);
    } catch (const pj::Error &) {
    }
    try {
        audioB.stopTransmit(audioA);
    } catch (const pj::Error &) {
    }
}

} // namespace

bool ConferenceBridge::conference(const QList<int> &callIds, CallRegistry &registry,
                                  QList<int> *affectedIds) {
    std::set<int> validIds;
    for (const int id : callIds) {
        if (registry.find(id) != nullptr) {
            validIds.insert(id);
        }
    }
    if (validIds.size() < 2) {
        return false;
    }

    members_.insert(validIds.begin(), validIds.end());
    for (const int id : members_) {
        SipCall *call = registry.find(id);
        if (call != nullptr && call->snapshot().localHold) {
            (void)call->setHoldState(false);
        }
        if (affectedIds != nullptr) {
            affectedIds->append(id);
        }
    }

    syncMedia(registry);
    return true;
}

bool ConferenceBridge::unconference(int callId, CallRegistry &registry, QList<int> *affectedIds) {
    if (members_.find(callId) == members_.end()) {
        return false;
    }

    SipCall *leaving = registry.find(callId);
    if (leaving != nullptr) {
        for (const int otherId : members_) {
            if (otherId == callId) {
                continue;
            }
            if (SipCall *other = registry.find(otherId)) {
                disconnectPairBidirectional(*leaving, *other);
            }
        }
    }

    members_.erase(callId);
    if (affectedIds != nullptr) {
        affectedIds->append(callId);
    }
    if (members_.size() < 2) {
        for (const int remId : members_) {
            if (affectedIds != nullptr) {
                affectedIds->append(remId);
            }
        }
        members_.clear();
    }
    return true;
}

QList<int> ConferenceBridge::removeDisconnected(int callId) {
    QList<int> affected;
    if (members_.erase(callId) == 0) {
        return affected;
    }
    if (members_.size() < 2) {
        for (const int remId : members_) {
            affected.append(remId);
        }
        members_.clear();
    }
    return affected;
}

void ConferenceBridge::syncMedia(CallRegistry &registry) {
    std::vector<SipCall *> activeCalls;
    activeCalls.reserve(members_.size());
    for (const int id : members_) {
        if (SipCall *call = registry.find(id)) {
            activeCalls.push_back(call);
        }
    }

    for (std::size_t i = 0; i < activeCalls.size(); ++i) {
        for (std::size_t j = i + 1; j < activeCalls.size(); ++j) {
            connectPairBidirectional(*activeCalls[i], *activeCalls[j]);
        }
    }
}

bool ConferenceBridge::isMember(int callId) const {
    return members_.find(callId) != members_.end();
}

void ConferenceBridge::clear() {
    members_.clear();
}

} // namespace tincan::sip
