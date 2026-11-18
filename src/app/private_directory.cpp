// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "app/private_directory.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <cerrno>

#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

namespace tincan {

namespace {

void setError(QString *error, const QString &message) {
    if (error != nullptr) {
        *error = message;
    }
}

} // namespace

bool PrivateDirectory::isPrivate(const QString &path, QString *error) {
    struct stat st {};
    if (::lstat(QFile::encodeName(path).constData(), &st) != 0) {
        setError(error, tr("%1 does not exist.").arg(path));
        return false;
    }
    if (!S_ISDIR(st.st_mode)) {
        setError(error, tr("%1 is not a directory.").arg(path));
        return false;
    }
    if (st.st_uid != ::geteuid()) {
        setError(error, tr("%1 is owned by another user.").arg(path));
        return false;
    }
    if ((st.st_mode & (S_IRWXG | S_IRWXO)) != 0) {
        setError(error, tr("%1 is accessible to other users.").arg(path));
        return false;
    }
    return true;
}

bool PrivateDirectory::ensure(const QString &path, QString *error) {
    const QString parent = QFileInfo(path).absolutePath();
    if (!QDir().mkpath(parent)) {
        setError(error, tr("Cannot create %1.").arg(parent));
        return false;
    }
    if (::mkdir(QFile::encodeName(path).constData(), S_IRWXU) != 0 && errno != EEXIST) {
        setError(error, tr("Cannot create %1.").arg(path));
        return false;
    }
    return isPrivate(path, error);
}

bool PrivateDirectory::isPeerSameUser(qintptr fd) {
    if (fd < 0) {
        return false;
    }
    struct ucred cred {};
    socklen_t len = sizeof(cred);
    if (::getsockopt(static_cast<int>(fd), SOL_SOCKET, SO_PEERCRED, &cred, &len) != 0 ||
        len != sizeof(cred)) {
        return false;
    }
    return cred.uid == ::geteuid();
}

} // namespace tincan
