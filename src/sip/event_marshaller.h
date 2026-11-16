// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include <QObject>

#include <functional>

namespace tincan::sip {

class EventMarshaller : public QObject {
    Q_OBJECT

public:
    explicit EventMarshaller(QObject *parent = nullptr);
    ~EventMarshaller() override = default;

    void post(std::function<void()> fn);
};

} // namespace tincan::sip
