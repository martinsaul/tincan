// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "app/single_instance.h"

#include "app/private_directory.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLocalServer>
#include <QLocalSocket>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <algorithm>

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

using tincan::CommandLine;
using tincan::SingleInstance;

namespace {

CommandLine dialCommand(const QString &target) {
    CommandLine cmd;
    cmd.action = CommandLine::Action::Dial;
    cmd.target = target;
    return cmd;
}

// Leaves a socket file with nobody listening, as a crashed primary would.
bool createStaleSocket(const QString &path) {
    const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        return false;
    }
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    const QByteArray encoded = QFile::encodeName(path);
    if (encoded.size() >= static_cast<qsizetype>(sizeof(addr.sun_path))) {
        ::close(fd);
        return false;
    }
    std::copy(encoded.cbegin(), encoded.cend(), addr.sun_path);
    const bool ok = ::bind(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) == 0;
    ::close(fd);
    return ok && QFile::exists(path);
}

// Restores the XDG variables a test overrides.
class EnvGuard {
public:
    EnvGuard()
        : runtime_(qgetenv("XDG_RUNTIME_DIR")), cache_(qgetenv("XDG_CACHE_HOME")),
          hadRuntime_(qEnvironmentVariableIsSet("XDG_RUNTIME_DIR")),
          hadCache_(qEnvironmentVariableIsSet("XDG_CACHE_HOME")) {}
    ~EnvGuard() {
        restore("XDG_RUNTIME_DIR", hadRuntime_, runtime_);
        restore("XDG_CACHE_HOME", hadCache_, cache_);
    }
    EnvGuard(const EnvGuard &) = delete;
    EnvGuard &operator=(const EnvGuard &) = delete;

private:
    static void restore(const char *name, bool had, const QByteArray &value) {
        if (had) {
            qputenv(name, value);
        } else {
            qunsetenv(name);
        }
    }

    QByteArray runtime_;
    QByteArray cache_;
    bool hadRuntime_;
    bool hadCache_;
};

} // namespace

class TestSingleInstance : public QObject {
    Q_OBJECT

private slots:
    void defaultNameIsPerUser() {
        QTemporaryDir cache;
        const EnvGuard guard;
        qunsetenv("XDG_RUNTIME_DIR");
        qputenv("XDG_CACHE_HOME", cache.path().toUtf8());
        const QString name = SingleInstance::defaultServerName();
        QVERIFY(name.endsWith(QStringLiteral("tincan-%1").arg(::geteuid())));
    }

    void defaultNameUsesPrivateRuntimeDir() {
        QTemporaryDir runtime; // QTemporaryDir is created 0700
        QVERIFY(runtime.isValid());
        const EnvGuard guard;
        qputenv("XDG_RUNTIME_DIR", runtime.path().toUtf8());
        QCOMPARE(QFileInfo(SingleInstance::defaultServerName()).absolutePath(),
                 QFileInfo(runtime.path()).absoluteFilePath());
    }

    void defaultNameFallsBackToPrivateCacheDir() {
        QTemporaryDir cache;
        QVERIFY(cache.isValid());
        const EnvGuard guard;
        qunsetenv("XDG_RUNTIME_DIR");
        qputenv("XDG_CACHE_HOME", cache.path().toUtf8());
        const QString name = SingleInstance::defaultServerName();
        const QString dir = QFileInfo(name).absolutePath();
        QVERIFY2(dir.startsWith(cache.path()), qPrintable(name));
        QVERIFY(tincan::PrivateDirectory::isPrivate(dir));
    }

    void sharedRuntimeDirIsNotUsed() {
        QTemporaryDir runtime;
        QTemporaryDir cache;
        QVERIFY(QFile::setPermissions(runtime.path(),
                                      QFile::permissions(runtime.path()) | QFile::ExeOther));
        const EnvGuard guard;
        qputenv("XDG_RUNTIME_DIR", runtime.path().toUtf8());
        qputenv("XDG_CACHE_HOME", cache.path().toUtf8());
        QVERIFY(SingleInstance::defaultServerName().startsWith(cache.path()));
    }

    void refusesSharedFallbackDir() {
        QTemporaryDir cache;
        const QString ipc = cache.filePath(QStringLiteral("tincan/ipc"));
        QVERIFY(QDir().mkpath(ipc));
        QVERIFY(QFile::setPermissions(ipc, QFile::permissions(ipc) | QFile::ReadOther |
                                               QFile::ExeOther));
        const EnvGuard guard;
        qunsetenv("XDG_RUNTIME_DIR");
        qputenv("XDG_CACHE_HOME", cache.path().toUtf8());
        QVERIFY(SingleInstance::defaultServerName().isEmpty());
        SingleInstance instance(QString{});
        QCOMPARE(instance.start(CommandLine{}), SingleInstance::Role::Failed);
        QVERIFY(!instance.errorString().isEmpty());
    }

    void secondInstanceForwardsToFirst() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString name = dir.filePath(QStringLiteral("instance.sock"));

        SingleInstance primary(name);
        QSignalSpy received(&primary, &SingleInstance::commandReceived);
        QCOMPARE(primary.start(CommandLine{}), SingleInstance::Role::Primary);
        QVERIFY(primary.isPrimary());
        const auto perms = QFile::permissions(name);
        QVERIFY(!(perms & (QFileDevice::ReadOther | QFileDevice::WriteOther)));

        const CommandLine forwarded = dialCommand(QStringLiteral("sip:123@host"));
        SingleInstance secondary(name);
        QCOMPARE(secondary.start(forwarded), SingleInstance::Role::Forwarded);
        QVERIFY(!secondary.isPrimary());

        QTRY_COMPARE(received.size(), 1);
        QCOMPARE(received.first().first().value<CommandLine>(), forwarded);
        QTest::qWait(50);
        QCOMPARE(received.size(), 1);
    }

    void forwardsSeveralCommandsInOrder() {
        QTemporaryDir dir;
        const QString name = dir.filePath(QStringLiteral("instance.sock"));
        SingleInstance primary(name);
        QSignalSpy received(&primary, &SingleInstance::commandReceived);
        QCOMPARE(primary.start(CommandLine{}), SingleInstance::Role::Primary);

        CommandLine answer;
        answer.action = CommandLine::Action::Answer;
        CommandLine hangup;
        hangup.action = CommandLine::Action::Hangup;
        QCOMPARE(SingleInstance(name).start(answer), SingleInstance::Role::Forwarded);
        QCOMPARE(SingleInstance(name).start(hangup), SingleInstance::Role::Forwarded);
        QTRY_COMPARE(received.size(), 2);
        QCOMPARE(received.at(0).first().value<CommandLine>(), answer);
        QCOMPARE(received.at(1).first().value<CommandLine>(), hangup);
    }

    void staleSocketIsReplaced() {
        QTemporaryDir dir;
        const QString name = dir.filePath(QStringLiteral("stale.sock"));
        QVERIFY(createStaleSocket(name));

        SingleInstance instance(name);
        QCOMPARE(instance.start(CommandLine{}), SingleInstance::Role::Primary);
        QVERIFY(instance.isPrimary());
    }

    void unresponsivePrimaryIsReported() {
        QTemporaryDir dir;
        const QString name = dir.filePath(QStringLiteral("mute.sock"));
        QLocalServer mute; // accepts connections but never answers
        QVERIFY(mute.listen(name));

        SingleInstance instance(name);
        QCOMPARE(instance.start(dialCommand(QStringLiteral("100")), 200),
                 SingleInstance::Role::Failed);
        QVERIFY(!instance.errorString().isEmpty());
    }

    void primaryIgnoresMalformedFrames() {
        QTemporaryDir dir;
        const QString name = dir.filePath(QStringLiteral("instance.sock"));
        SingleInstance primary(name);
        QSignalSpy received(&primary, &SingleInstance::commandReceived);
        QCOMPARE(primary.start(CommandLine{}), SingleInstance::Role::Primary);

        QLocalSocket client;
        client.connectToServer(name);
        QVERIFY(client.waitForConnected(1000));
        client.write(SingleInstance::encodeFrame(QByteArrayLiteral("{\"action\":\"boom\"}")));
        client.flush();
        QTRY_COMPARE(client.state(), QLocalSocket::UnconnectedState);
        QCOMPARE(received.size(), 0);
    }
};

QTEST_GUILESS_MAIN(TestSingleInstance)
#include "tst_singleinstance.moc"
