// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include <QList>

#include <set>

namespace tincan::sip {

class CallRegistry;

class ConferenceBridge {
public:
    ConferenceBridge() = default;
    ~ConferenceBridge() = default;

    ConferenceBridge(const ConferenceBridge &) = delete;
    ConferenceBridge &operator=(const ConferenceBridge &) = delete;

    bool conference(const QList<int> &callIds, CallRegistry &registry, QList<int> *affectedIds);
    bool unconference(int callId, CallRegistry &registry, QList<int> *affectedIds);
    QList<int> removeDisconnected(int callId);
    void syncMedia(CallRegistry &registry);
    bool isMember(int callId) const;
    void clear();

private:
    std::set<int> members_;
};

} // namespace tincan::sip
