// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "sip/event_marshaller.h"

#include <QMetaObject>

#include <utility>

namespace tincan::sip {

EventMarshaller::EventMarshaller(QObject *parent) : QObject(parent) {}

void EventMarshaller::post(std::function<void()> fn) {
    if (!fn) {
        return;
    }
    QMetaObject::invokeMethod(this, std::move(fn), Qt::QueuedConnection);
}

} // namespace tincan::sip
