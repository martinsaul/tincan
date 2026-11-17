// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "app/app_paths.h"

#include <QDir>
#include <QStandardPaths>

namespace tincan {

QString AppPaths::configDir() {
    const QString base = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    return QDir(base).filePath(QStringLiteral("tincan"));
}

QString AppPaths::dataDir() {
    const QString base = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    return QDir(base).filePath(QStringLiteral("tincan"));
}

QString AppPaths::recordingsDir() {
    const QString music = QStandardPaths::writableLocation(QStandardPaths::MusicLocation);
    if (!music.trimmed().isEmpty()) {
        return QDir(music).filePath(QStringLiteral("Tincan"));
    }
    return QDir(dataDir()).filePath(QStringLiteral("Recordings"));
}

} // namespace tincan
