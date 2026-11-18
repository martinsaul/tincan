// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include <QDateTime>
#include <QString>

namespace tincan::RecordingFile {

// Recordings hold private call audio. Creates a new, empty, owner-only (0600)
// WAV file for a call in `recordingsDir` and returns its path, or an empty
// string on failure. The directory is created with mode 0700 if it is
// missing; an existing directory is left as the user configured it. An
// existing file is never reused or overwritten: a "-2", "-3"... suffix is
// added instead, and symlinks are not followed.
QString create(const QString &recordingsDir, const QString &remoteUri,
               const QDateTime &timestamp = QDateTime::currentDateTime());

// Removes `path` if it is an empty file, i.e. a reservation from create()
// that never received audio.
void discardIfEmpty(const QString &path);

} // namespace tincan::RecordingFile
