// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include "app/notification_backend.h"

#include <QString>

#include <optional>

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

    // The specification lets servers with the "body-markup" capability
    // interpret the body as markup. Bodies carry caller-controlled text
    // (display names, URIs, messages), so they are escaped for such servers.
    static QString bodyFor(const QString &text, bool serverUsesMarkup);

private slots:
    void onActionInvoked(uint id, const QString &actionKey);
    void onNotificationClosed(uint id, uint reason);

private:
    void subscribeToSignals();
    bool serverUsesMarkup();

    QString appName_;
    QString appIcon_;
    bool subscribed_ = false;
    std::optional<bool> bodyMarkup_;
};

} // namespace tincan
