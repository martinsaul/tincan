// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include "app/command_line.h"

#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QString>

class QLocalServer;
class QLocalSocket;

namespace tincan {

// Ensures one Tincan process per user. The first process listens on a
// user-private local socket; later processes forward their CommandLine to it
// (length-prefixed JSON, acknowledged by the primary) and exit. The socket
// lives in a directory only this user can enter, both ends check that the
// peer runs as the same user, and a lock file serialises start-up so two
// processes launched together cannot both become primary.
class SingleInstance : public QObject {
    Q_OBJECT

public:
    enum class Role { Primary, Forwarded, Failed };

    explicit SingleInstance(QString serverName = defaultServerName(), QObject *parent = nullptr);
    ~SingleInstance() override;

    SingleInstance(const SingleInstance &) = delete;
    SingleInstance &operator=(const SingleInstance &) = delete;

    // Forwards `command` to a running primary, or becomes the primary.
    Role start(const CommandLine &command, int ackTimeoutMs = 2000);

    bool isPrimary() const;
    QString serverName() const;
    QString errorString() const;

    // $XDG_RUNTIME_DIR/tincan-<uid> if that directory is private, else a
    // 0700 directory under the user's cache dir. Empty if neither is usable.
    static QString defaultServerName();
    static QByteArray encodeFrame(const QByteArray &payload);

signals:
    void commandReceived(const tincan::CommandLine &command);

private:
    enum class ForwardResult { Acked, NoServer, NoAck, WrongUser };

    Role startLocked(const CommandLine &command, int ackTimeoutMs);
    ForwardResult forward(const CommandLine &command, int ackTimeoutMs);
    QString lockFilePath() const;
    bool listen();
    void onNewConnection();
    void onClientReadyRead(QLocalSocket *socket);

    QString serverName_;
    QString errorString_;
    QLocalServer *server_ = nullptr;
    QHash<QLocalSocket *, QByteArray> buffers_;
};

} // namespace tincan
