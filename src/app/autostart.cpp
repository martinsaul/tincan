// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "app/autostart.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>

#include <utility>

namespace tincan {

namespace {

const QString kFileName = QStringLiteral("io.github.martinsaul.Tincan.desktop");

} // namespace

Autostart::Autostart(QString autostartDir) : autostartDir_(std::move(autostartDir)) {}

QString Autostart::defaultDirectory() {
    const QString config = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    return QDir(config).filePath(QStringLiteral("autostart"));
}

QByteArray Autostart::desktopEntry() {
    return QByteArrayLiteral("[Desktop Entry]\n"
                             "Type=Application\n"
                             "Name=Tincan\n"
                             "Comment=SIP softphone\n"
                             "Exec=tincan --minimized\n"
                             "Icon=io.github.martinsaul.Tincan\n"
                             "Terminal=false\n"
                             "Categories=Network;Telephony;\n"
                             "X-GNOME-Autostart-enabled=true\n");
}

QString Autostart::filePath() const {
    const QString dir = autostartDir_.isEmpty() ? defaultDirectory() : autostartDir_;
    return QDir(dir).filePath(kFileName);
}

bool Autostart::isEnabled() const { return QFile::exists(filePath()); }

bool Autostart::apply(bool enabled) {
    const QString path = filePath();
    if (!enabled) {
        return !QFile::exists(path) || QFile::remove(path);
    }
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        return false;
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    file.write(desktopEntry());
    return file.commit();
}

} // namespace tincan
