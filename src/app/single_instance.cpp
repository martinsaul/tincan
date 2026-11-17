// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "app/single_instance.h"

#include <QDir>
#include <QEventLoop>
#include <QFileInfo>
#include <QLocalServer>
#include <QLocalSocket>
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

} // namespace

SingleInstance::SingleInstance(QString serverName, QObject *parent)
    : QObject(parent), serverName_(std::move(serverName)) {}

SingleInstance::~SingleInstance() {
    if (server_ != nullptr) {
        server_->close();
    }
}

QString SingleInstance::defaultServerName() {
    const QString base = QStringLiteral("tincan-%1").arg(::getuid());
    const QString runtimeDir = qEnvironmentVariable("XDG_RUNTIME_DIR");
    if (!runtimeDir.isEmpty() && QFileInfo(runtimeDir).isDir()) {
        return QDir(runtimeDir).filePath(base);
    }
    return base;
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
    for (int attempt = 0; attempt < 2; ++attempt) {
        const ForwardResult forwarded = forward(command, ackTimeoutMs);
        if (forwarded == ForwardResult::Acked) {
            return Role::Forwarded;
        }
        if (forwarded == ForwardResult::NoAck) {
            errorString_ = tr("The running Tincan instance did not respond.");
            return Role::Failed;
        }
        // Nothing answered: any socket file left behind is stale.
        QLocalServer::removeServer(serverName_);
        if (listen()) {
            return Role::Primary;
        }
    }
    return Role::Failed;
}

SingleInstance::ForwardResult SingleInstance::forward(const CommandLine &command,
                                                      int ackTimeoutMs) {
    QLocalSocket socket;
    socket.connectToServer(serverName_);
    if (!socket.waitForConnected(kConnectTimeoutMs)) {
        return ForwardResult::NoServer;
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
