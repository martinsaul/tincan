// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include "app/notification_backend.h"

#include <QString>

namespace tincan {

// org.freedesktop.Notifications client on the session bus.
class DbusNotifications : public INotificationBackend {
    Q_OBJECT

public:
    explicit DbusNotifications(QString appName = QStringLiteral("Tincan"),
                               QString appIcon = QStringLiteral("io.github.martinsaul.Tincan"),
                               QObject *parent = nullptr);

    bool isAvailable() const override;
    uint notify(const tincan::NotificationRequest &request) override;
    void close(uint id) override;

private slots:
    void onActionInvoked(uint id, const QString &actionKey);
    void onNotificationClosed(uint id, uint reason);

private:
    void subscribeToSignals();

    QString appName_;
    QString appIcon_;
    bool subscribed_ = false;
};

} // namespace tincan
