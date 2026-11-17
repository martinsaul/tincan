// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include <QDateTime>
#include <QString>

namespace tincan::RecordingNamer {

QString buildWavPath(const QString &recordingsDir, const QString &remoteUri,
                     const QDateTime &timestamp = QDateTime::currentDateTime());

} // namespace tincan::RecordingNamer
