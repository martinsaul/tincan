// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "ui/icons.h"

#include <QPainter>
#include <QPolygonF>
#include <QSvgRenderer>

static void initTincanUiResources() {
    Q_INIT_RESOURCE(tincan);
}

namespace tincan::ui::Icons {

namespace {

QIcon themedOrFallback(const QString &themeName, const QString &qrcPath) {
    ensureInitialized();
    const QIcon fallback(qrcPath);
    const QIcon themed = QIcon::fromTheme(themeName, fallback);
    return themed.isNull() ? fallback : themed;
}

QPixmap drawCirclePixmap(const QColor &fill, int diameter) {
    QPixmap pix(diameter, diameter);
    pix.fill(Qt::transparent);
    QPainter painter(&pix);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(fill.darker(130), 1.0));
    painter.setBrush(fill);
    painter.drawEllipse(QRectF(1.0, 1.0, diameter - 2.0, diameter - 2.0));
    return pix;
}

} // namespace

void ensureInitialized() {
    static const bool initialized = []() {
        initTincanUiResources();
        return true;
    }();
    (void)initialized;
}

QIcon appIcon() {
    ensureInitialized();
    return QIcon(QStringLiteral(":/icons/tincan.svg"));
}

QIcon callStart() {
    return themedOrFallback(QStringLiteral("call-start"), QStringLiteral(":/icons/call-start.svg"));
}

QIcon callStop() {
    return themedOrFallback(QStringLiteral("call-stop"), QStringLiteral(":/icons/call-stop.svg"));
}

QIcon microphone() {
    return themedOrFallback(QStringLiteral("audio-input-microphone"),
                            QStringLiteral(":/icons/audio-input-microphone.svg"));
}

QIcon speakers() {
    return themedOrFallback(QStringLiteral("audio-speakers"),
                            QStringLiteral(":/icons/audio-speakers.svg"));
}

QIcon settings() {
    return themedOrFallback(QStringLiteral("preferences-system"),
                            QStringLiteral(":/icons/preferences-system.svg"));
}

QIcon backspace() {
    return themedOrFallback(QStringLiteral("edit-clear"), QStringLiteral(":/icons/edit-clear.svg"));
}

QColor regStateColor(RegState state) {
    switch (state) {
    case RegState::Unregistered:
        return QColor(138, 143, 152);
    case RegState::Registering:
        return QColor(229, 160, 13);
    case RegState::Registered:
        return QColor(46, 160, 67);
    case RegState::Failed:
        return QColor(217, 48, 37);
    }
    return QColor(138, 143, 152);
}

QPixmap statusDotPixmap(RegState state, int diameter) {
    return drawCirclePixmap(regStateColor(state), diameter);
}

QIcon trayIconForState(RegState state) {
    ensureInitialized();
    QPixmap pix(64, 64);
    pix.fill(Qt::transparent);
    QPainter painter(&pix);
    painter.setRenderHint(QPainter::Antialiasing, true);

    QSvgRenderer renderer(QStringLiteral(":/icons/tincan.svg"));
    renderer.render(&painter, QRectF(0.0, 0.0, 64.0, 64.0));

    painter.setPen(QPen(Qt::white, 3.0));
    painter.setBrush(regStateColor(state));
    painter.drawEllipse(QRectF(42.0, 42.0, 18.0, 18.0));
    return QIcon(pix);
}

QColor presenceStateColor(PresenceState state) {
    switch (state) {
    case PresenceState::Unknown:
        return QColor(148, 163, 184);
    case PresenceState::Offline:
        return QColor(100, 116, 139);
    case PresenceState::Online:
        return QColor(22, 163, 74);
    case PresenceState::Away:
        return QColor(217, 119, 6);
    case PresenceState::Busy:
        return QColor(220, 38, 38);
    case PresenceState::Ringing:
        return QColor(249, 115, 22);
    case PresenceState::OnThePhone:
        return QColor(185, 28, 28);
    }
    return QColor(148, 163, 184);
}

QPixmap presenceDotPixmap(PresenceState state, int diameter) {
    return drawCirclePixmap(presenceStateColor(state), diameter);
}

QIcon presenceDotIcon(PresenceState state, int diameter) {
    return QIcon(presenceDotPixmap(state, diameter));
}

QIcon historyDirectionIcon(CallDirection direction, bool missed) {
    QPixmap pix(16, 16);
    pix.fill(Qt::transparent);
    QPainter painter(&pix);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QColor color = missed ? QColor(220, 38, 38)
                                : (direction == CallDirection::Incoming ? QColor(22, 163, 74)
                                                                        : QColor(37, 99, 235));
    QPen pen(color, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter.setPen(pen);

    if (direction == CallDirection::Outgoing) {
        painter.drawLine(QPointF(4.0, 12.0), QPointF(12.0, 4.0));
        painter.drawLine(QPointF(7.0, 4.0), QPointF(12.0, 4.0));
        painter.drawLine(QPointF(12.0, 4.0), QPointF(12.0, 9.0));
    } else {
        painter.drawLine(QPointF(12.0, 4.0), QPointF(4.0, 12.0));
        painter.drawLine(QPointF(4.0, 7.0), QPointF(4.0, 12.0));
        painter.drawLine(QPointF(4.0, 12.0), QPointF(9.0, 12.0));
    }
    return QIcon(pix);
}

} // namespace tincan::ui::Icons
