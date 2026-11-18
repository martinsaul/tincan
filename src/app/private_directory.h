// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include <QCoreApplication>
#include <QString>

#include <QtGlobal>

namespace tincan {

// POSIX checks that keep per-user IPC endpoints out of other users' reach.
class PrivateDirectory {
    Q_DECLARE_TR_FUNCTIONS(PrivateDirectory)

public:
    // True if `path` is a real directory (not a symlink), owned by the
    // effective user and with no group or other permission bits.
    static bool isPrivate(const QString &path, QString *error = nullptr);

    // Creates `path` with mode 0700 if it is missing (parents get default
    // permissions) and then verifies it with isPrivate(). An existing
    // directory is never loosened or repaired; a bad one is refused.
    static bool ensure(const QString &path, QString *error = nullptr);

    // True if the process at the other end of the connected Unix socket
    // `fd` runs as the effective user (SO_PEERCRED).
    static bool isPeerSameUser(qintptr fd);
};

} // namespace tincan
