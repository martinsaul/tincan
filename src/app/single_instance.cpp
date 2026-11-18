// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "app/single_instance.h"

#include "app/private_directory.h"

#include <QDir>
#include <QEventLoop>
#include <QFileInfo>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLockFile>
#include <QStandardPaths>
#include <QTimer>
#include <QtEndian>

#include <unistd.h>

#include <utility>

namespace tincan {

namespace {

constexpr qsizetype kHeaderSize = 4;
constexpr quint32 kMaxPayload = 64 * 1024;
constexpr char kAck = '\x06';
constexpr int kConnectTimeoutMs = 500;
// Covers a competing start-up's connect timeout plus its ack timeout.
constexpr int kLockSlackMs = 3000;

QString fallbackSocketDir() {
    const QString cache = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation);
    if (cache.isEmpty()) {
        return {};
    }
    return QDir(cache).filePath(QStringLiteral("tincan/ipc"));
}

} // namespace

SingleInstance::SingleInstance(QString serverName, QObject *parent)
    : QObject(parent), serverName_(std::move(serverName)) {}

SingleInstance::~SingleInstance() {
    if (server_ != nullptr) {
        server_->close();
    }
}

QString SingleInstance::defaultServerName() {
    const QString base = QStringLiteral("tincan-%1").arg(::geteuid());
    const QString runtimeDir = qEnvironmentVariable("XDG_RUNTIME_DIR");
    if (!runtimeDir.isEmpty() && PrivateDirectory::isPrivate(runtimeDir)) {
        return QDir(runtimeDir).filePath(base);
    }
    // Never fall back to a shared directory such as /tmp: another local user
    // could create the socket first and receive our forwarded commands.
    const QString fallback = fallbackSocketDir();
    if (!fallback.isEmpty() && PrivateDirectory::ensure(fallback)) {
        return QDir(fallback).filePath(base);
    }
    return {};
}

QByteArray SingleInstance::encodeFrame(const QByteArray &payload) {
    QByteArray frame(kHeaderSize, '\0');
    qToBigEndian(static_cast<quint32>(payload.size()), frame.data());
    frame.append(payload);
    return frame;
}

bool SingleInstance::isPrimary() const { return server_ != nullptr && server_->isListening(); }

QString SingleInstance::serverName() const { return serverName_; }

QString SingleInstance::errorString() const { return errorString_; }

SingleInstance::Role SingleInstance::start(const CommandLine &command, int ackTimeoutMs) {
    if (serverName_.isEmpty()) {
        errorString_ = tr("No private directory is available for the instance socket.");
        return Role::Failed;
    }
    QLockFile lock(lockFilePath());
    if (!lock.tryLock(ackTimeoutMs + kLockSlackMs)) {
        errorString_ = tr("Another Tincan instance is starting.");
        return Role::Failed;
    }
    return startLocked(command, ackTimeoutMs);
}

SingleInstance::Role SingleInstance::startLocked(const CommandLine &command, int ackTimeoutMs) {
    switch (forward(command, ackTimeoutMs)) {
    case ForwardResult::Acked:
        return Role::Forwarded;
    case ForwardResult::NoAck:
        errorString_ = tr("The running Tincan instance did not respond.");
        return Role::Failed;
    case ForwardResult::WrongUser:
        errorString_ = tr("The instance socket belongs to another user.");
        return Role::Failed;
    case ForwardResult::NoServer:
        break;
    }
    // Nothing answered and nobody else can be starting (we hold the lock), so
    // any socket file left behind is stale.
    QLocalServer::removeServer(serverName_);
    return listen() ? Role::Primary : Role::Failed;
}

QString SingleInstance::lockFilePath() const {
    const QString name = serverName_ + QStringLiteral(".lock");
    return QDir::isAbsolutePath(name) ? name : QDir::temp().filePath(name);
}

SingleInstance::ForwardResult SingleInstance::forward(const CommandLine &command,
                                                      int ackTimeoutMs) {
    QLocalSocket socket;
    socket.connectToServer(serverName_);
    if (!socket.waitForConnected(kConnectTimeoutMs)) {
        return ForwardResult::NoServer;
    }
    if (!PrivateDirectory::isPeerSameUser(socket.socketDescriptor())) {
        socket.abort();
        return ForwardResult::WrongUser;
    }
    socket.write(encodeFrame(command.toJson()));

    // A local event loop (rather than waitForReadyRead) keeps a primary that
    // lives in this same thread able to answer; tests rely on that.
    bool acked = false;
    QEventLoop loop;
    QTimer::singleShot(ackTimeoutMs, &loop, &QEventLoop::quit);
    connect(&socket, &QLocalSocket::readyRead, &loop, [&]() {
        acked = socket.readAll().contains(kAck);
        loop.quit();
    });
    connect(&socket, &QLocalSocket::disconnected, &loop, &QEventLoop::quit);
    loop.exec();
    socket.abort();
    return acked ? ForwardResult::Acked : ForwardResult::NoAck;
}

bool SingleInstance::listen() {
    delete server_;
    server_ = new QLocalServer(this);
    server_->setSocketOptions(QLocalServer::UserAccessOption);
    if (!server_->listen(serverName_)) {
        errorString_ = server_->errorString();
        delete server_;
        server_ = nullptr;
        return false;
    }
    connect(server_, &QLocalServer::newConnection, this, &SingleInstance::onNewConnection);
    return true;
}

void SingleInstance::onNewConnection() {
    while (QLocalSocket *socket = server_->nextPendingConnection()) {
        if (!PrivateDirectory::isPeerSameUser(socket->socketDescriptor())) {
            socket->abort();
            socket->deleteLater();
            continue;
        }
        buffers_.insert(socket, {});
        connect(socket, &QLocalSocket::readyRead, this,
                [this, socket]() { onClientReadyRead(socket); });
        connect(socket, &QLocalSocket::disconnected, this, [this, socket]() {
            buffers_.remove(socket);
            socket->deleteLater();
        });
        if (socket->bytesAvailable() > 0) {
            onClientReadyRead(socket);
        }
    }
}

void SingleInstance::onClientReadyRead(QLocalSocket *socket) {
    QByteArray &buffer = buffers_[socket];
    buffer.append(socket->readAll());
    if (buffer.size() < kHeaderSize) {
        return;
    }
    const quint32 length = qFromBigEndian<quint32>(buffer.constData());
    if (length > kMaxPayload) {
        socket->abort();
        return;
    }
    if (buffer.size() < kHeaderSize + static_cast<qsizetype>(length)) {
        return;
    }
    const std::optional<CommandLine> command =
        CommandLine::fromJson(buffer.mid(kHeaderSize, length));
    buffer.clear();
    if (!command) {
        socket->abort();
        return;
    }
    socket->write(QByteArray(1, kAck));
    socket->flush();
    socket->disconnectFromServer();
    emit commandReceived(*command);
}

} // namespace tincan
