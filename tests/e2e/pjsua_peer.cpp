// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "pjsua_peer.h"

#include <QHostAddress>
#include <QStringList>
#include <QTest>
#include <QUdpSocket>

namespace tincan::test {

PjsuaPeer::PjsuaPeer() {
    process_.setProcessChannelMode(QProcess::MergedChannels);
    QObject::connect(&process_, &QProcess::readyReadStandardOutput, &process_, [this]() {
        capturedOutput_.append(process_.readAllStandardOutput());
    });
}

PjsuaPeer::~PjsuaPeer() {
    stop();
}

int PjsuaPeer::allocateFreeUdpPort() {
    QUdpSocket socket;
    if (!socket.bind(QHostAddress::LocalHost, 0)) {
        return 0;
    }
    const int port = static_cast<int>(socket.localPort());
    socket.close();
    return port;
}

bool PjsuaPeer::start(int autoAnswerCode, int durationSec, const QString &destinationUri) {
    for (int attempt = 0; attempt < 3; ++attempt) {
        if (tryStartOnce(autoAnswerCode, durationSec, destinationUri)) {
            return true;
        }
        stop();
    }
    return false;
}

void PjsuaPeer::setExtraArgs(const QStringList &args) { extraArgs_ = args; }

void PjsuaPeer::sendInput(const QByteArray &data) {
    if (process_.state() == QProcess::Running) {
        process_.write(data);
        process_.waitForBytesWritten(1000);
    }
}

bool PjsuaPeer::tryStartOnce(int autoAnswerCode, int durationSec, const QString &destinationUri) {
    capturedOutput_.clear();
    localPort_ = allocateFreeUdpPort();
    if (localPort_ <= 0) {
        return false;
    }

    QStringList args{
        QStringLiteral("--null-audio"),
        QStringLiteral("--local-port=%1").arg(localPort_),
        QStringLiteral("--no-tcp"),
        QStringLiteral("--auto-answer=%1").arg(autoAnswerCode),
        QStringLiteral("--max-calls=4"),
        QStringLiteral("--log-level=4"),
        QStringLiteral("--app-log-level=4"),
        QStringLiteral("--stdout-no-buf"),
        QStringLiteral("--no-vad"),
        QStringLiteral("--duration=%1").arg(durationSec),
    };
    args.append(extraArgs_);
    if (!destinationUri.isEmpty()) {
        args.push_back(destinationUri);
    }

    process_.start(QStringLiteral(TINCAN_PJSUA_BINARY), args, QIODevice::ReadWrite);
    if (!process_.waitForStarted(5000)) {
        return false;
    }

    const bool ready = QTest::qWaitFor(
        [this]() {
            return capturedOutput_.contains("Ready: Success") ||
                   capturedOutput_.contains(">>>>") || process_.state() != QProcess::Running;
        },
        10000);
    return ready && process_.state() == QProcess::Running;
}

void PjsuaPeer::stop() {
    if (process_.state() == QProcess::NotRunning) {
        return;
    }
    process_.write("q\n");
    process_.waitForBytesWritten(1000);
    if (!process_.waitForFinished(3000)) {
        process_.kill();
        process_.waitForFinished(1000);
    }
    capturedOutput_.append(process_.readAllStandardOutput());
}

int PjsuaPeer::localPort() const { return localPort_; }

QString PjsuaPeer::sipUri() const { return QStringLiteral("sip:127.0.0.1:%1").arg(localPort_); }

QString PjsuaPeer::output() const { return QString::fromUtf8(capturedOutput_); }

} // namespace tincan::test
