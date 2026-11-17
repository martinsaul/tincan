// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "ui/icons.h"

#include <QPainter>
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
    QPixmap pix(diameter, diameter);
    pix.fill(Qt::transparent);
    QPainter painter(&pix);
    painter.setRenderHint(QPainter::Antialiasing, true);
    const QColor fill = regStateColor(state);
    painter.setPen(QPen(fill.darker(130), 1.0));
    painter.setBrush(fill);
    painter.drawEllipse(QRectF(1.0, 1.0, diameter - 2.0, diameter - 2.0));
    return pix;
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

} // namespace tincan::ui::Icons
