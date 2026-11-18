// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "app/recording_file.h"

#include "app/recording_namer.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <cerrno>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace tincan::RecordingFile {

namespace {

constexpr int kMaxAttempts = 100;

bool ensureDirectory(const QString &dir) {
    const QFileInfo info(dir);
    if (info.isDir()) {
        return true;
    }
    if (!QDir().mkpath(info.absolutePath())) {
        return false;
    }
    const QByteArray native = QFile::encodeName(dir);
    return ::mkdir(native.constData(), S_IRWXU) == 0 || errno == EEXIST;
}

QString withSuffix(const QString &path, int attempt) {
    if (attempt <= 1) {
        return path;
    }
    QString stem = path;
    stem.chop(4); // ".wav"
    return QStringLiteral("%1-%2.wav").arg(stem).arg(attempt);
}

// 1 = created, 0 = name taken, -1 = hard failure.
int createExclusive(const QString &path) {
    const QByteArray native = QFile::encodeName(path);
    const int fd = ::open(native.constData(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC,
                          S_IRUSR | S_IWUSR);
    if (fd < 0) {
        return errno == EEXIST ? 0 : -1;
    }
    // The umask can only remove bits; this guards against an unusual ACL default.
    (void)::fchmod(fd, S_IRUSR | S_IWUSR);
    ::close(fd);
    return 1;
}

} // namespace

QString create(const QString &recordingsDir, const QString &remoteUri,
               const QDateTime &timestamp) {
    if (recordingsDir.trimmed().isEmpty() || !ensureDirectory(recordingsDir)) {
        return {};
    }
    const QString base = RecordingNamer::buildWavPath(recordingsDir, remoteUri, timestamp);
    for (int attempt = 1; attempt <= kMaxAttempts; ++attempt) {
        const QString candidate = withSuffix(base, attempt);
        const int result = createExclusive(candidate);
        if (result == 1) {
            return candidate;
        }
        if (result < 0) {
            return {};
        }
    }
    return {};
}

void discardIfEmpty(const QString &path) {
    const QFileInfo info(path);
    if (!path.isEmpty() && info.isFile() && !info.isSymLink() && info.size() == 0) {
        (void)QFile::remove(path);
    }
}

} // namespace tincan::RecordingFile
