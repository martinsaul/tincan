// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "app/dbus_notifications.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusReply>
#include <QVariantMap>

#include <utility>

namespace tincan {

namespace {

const QString kService = QStringLiteral("org.freedesktop.Notifications");
const QString kPath = QStringLiteral("/org/freedesktop/Notifications");
const QString kInterface = QStringLiteral("org.freedesktop.Notifications");
constexpr int kCallTimeoutMs = 2000;

bool isActivatable(const QDBusConnection &bus) {
    const QDBusMessage call = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.DBus"), QStringLiteral("/org/freedesktop/DBus"),
        QStringLiteral("org.freedesktop.DBus"), QStringLiteral("ListActivatableNames"));
    const QDBusReply<QStringList> reply = bus.call(call, QDBus::Block, kCallTimeoutMs);
    return reply.isValid() && reply.value().contains(kService);
}

QVariantMap hintsFor(const NotificationRequest &request) {
    QVariantMap hints;
    hints.insert(QStringLiteral("desktop-entry"), QStringLiteral("io.github.martinsaul.Tincan"));
    hints.insert(QStringLiteral("urgency"),
                 QVariant::fromValue(static_cast<uchar>(request.urgency)));
    if (!request.category.isEmpty()) {
        hints.insert(QStringLiteral("category"), request.category);
    }
    if (request.persistent) {
        hints.insert(QStringLiteral("resident"), true);
    }
    return hints;
}

} // namespace

DbusNotifications::DbusNotifications(QString appName, QString appIcon, QObject *parent)
    : INotificationBackend(parent), appName_(std::move(appName)), appIcon_(std::move(appIcon)) {}

bool DbusNotifications::isAvailable() const {
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected() || bus.interface() == nullptr) {
        return false;
    }
    return bus.interface()->isServiceRegistered(kService).value() || isActivatable(bus);
}

void DbusNotifications::subscribeToSignals() {
    if (subscribed_) {
        return;
    }
    QDBusConnection bus = QDBusConnection::sessionBus();
    bus.connect(kService, kPath, kInterface, QStringLiteral("ActionInvoked"), this,
                SLOT(onActionInvoked(uint, QString)));
    bus.connect(kService, kPath, kInterface, QStringLiteral("NotificationClosed"), this,
                SLOT(onNotificationClosed(uint, uint)));
    subscribed_ = true;
}

uint DbusNotifications::notify(const NotificationRequest &request) {
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        return 0;
    }
    subscribeToSignals();
    QDBusMessage call = QDBusMessage::createMethodCall(kService, kPath, kInterface,
                                                       QStringLiteral("Notify"));
    const int expireTimeout = request.persistent ? 0 : -1;
    call << appName_ << uint{0} << appIcon_ << request.summary << request.body << request.actions
         << hintsFor(request) << expireTimeout;
    const QDBusReply<uint> reply = bus.call(call, QDBus::Block, kCallTimeoutMs);
    return reply.isValid() ? reply.value() : 0;
}

void DbusNotifications::close(uint id) {
    if (id == 0) {
        return;
    }
    QDBusMessage call = QDBusMessage::createMethodCall(kService, kPath, kInterface,
                                                       QStringLiteral("CloseNotification"));
    call << id;
    QDBusConnection::sessionBus().asyncCall(call);
}

void DbusNotifications::onActionInvoked(uint id, const QString &actionKey) {
    emit actionInvoked(id, actionKey);
}

void DbusNotifications::onNotificationClosed(uint id, uint reason) {
    Q_UNUSED(reason);
    emit notificationClosed(id);
}

} // namespace tincan
