// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

namespace tincan {

struct NotificationRequest {
    enum class Urgency { Low, Normal, Critical };

    QString summary;
    QString body;
    // Flat list of (key, label) pairs, as in the freedesktop specification.
    QStringList actions;
    QString category;
    Urgency urgency = Urgency::Normal;
    bool persistent = false;
};

// A desktop notification service. Implemented over D-Bus in production and
// faked in tests.
class INotificationBackend : public QObject {
    Q_OBJECT

public:
    using QObject::QObject;
    ~INotificationBackend() override = default;

    virtual bool isAvailable() const = 0;
    // Returns the notification id, or 0 on failure.
    virtual uint notify(const tincan::NotificationRequest &request) = 0;
    virtual void close(uint id) = 0;

signals:
    void actionInvoked(uint id, const QString &actionKey);
    void notificationClosed(uint id);
};

} // namespace tincan
