// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include "core/sip_events.h"

#include <QList>

namespace tincan {

class SipEventRouter : public ISipEvents {
public:
    void setTarget(ISipEvents *target) {
        listeners_.clear();
        if (target != nullptr) {
            listeners_.append(target);
        }
    }

    void addListener(ISipEvents *listener) {
        if (listener != nullptr && !listeners_.contains(listener)) {
            listeners_.append(listener);
        }
    }

    void clear() { listeners_.clear(); }

    void onRegistrationChanged(const RegistrationInfo &info) override {
        const QList<ISipEvents *> copy = listeners_;
        for (ISipEvents *listener : copy) {
            listener->onRegistrationChanged(info);
        }
    }

    void onIncomingCall(const CallInfo &info) override {
        const QList<ISipEvents *> copy = listeners_;
        for (ISipEvents *listener : copy) {
            listener->onIncomingCall(info);
        }
    }

    void onCallChanged(const CallInfo &info) override {
        const QList<ISipEvents *> copy = listeners_;
        for (ISipEvents *listener : copy) {
            listener->onCallChanged(info);
        }
    }

    void onPresenceChanged(const PresenceInfo &info) override {
        const QList<ISipEvents *> copy = listeners_;
        for (ISipEvents *listener : copy) {
            listener->onPresenceChanged(info);
        }
    }

    void onIncomingMessage(const MessageInfo &info) override {
        const QList<ISipEvents *> copy = listeners_;
        for (ISipEvents *listener : copy) {
            listener->onIncomingMessage(info);
        }
    }

    void onMessageStatus(quint64 token, int statusCode, const QString &reason) override {
        const QList<ISipEvents *> copy = listeners_;
        for (ISipEvents *listener : copy) {
            listener->onMessageStatus(token, statusCode, reason);
        }
    }

private:
    QList<ISipEvents *> listeners_;
};

} // namespace tincan
