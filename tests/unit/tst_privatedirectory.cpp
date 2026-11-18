// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "app/private_directory.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

using tincan::PrivateDirectory;

namespace {

mode_t modeOf(const QString &path) {
    struct stat st {};
    if (::lstat(QFile::encodeName(path).constData(), &st) != 0) {
        return 0;
    }
    return st.st_mode & 07777;
}

bool chmodPath(const QString &path, mode_t mode) {
    return ::chmod(QFile::encodeName(path).constData(), mode) == 0;
}

} // namespace

class TestPrivateDirectory : public QObject {
    Q_OBJECT

private slots:
    void ensureCreatesOwnerOnlyDirectory() {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const QString path = root.filePath(QStringLiteral("a/b/ipc"));
        QString error;
        QVERIFY2(PrivateDirectory::ensure(path, &error), qPrintable(error));
        QCOMPARE(modeOf(path), static_cast<mode_t>(0700));
        QVERIFY(PrivateDirectory::isPrivate(path));
        // A second call accepts the directory it made.
        QVERIFY(PrivateDirectory::ensure(path));
    }

    void refusesGroupOrWorldAccess_data() {
        QTest::addColumn<int>("mode");
        QTest::newRow("group-read") << 0740;
        QTest::newRow("group-exec") << 0710;
        QTest::newRow("world-exec") << 0701;
        QTest::newRow("world-all") << 0777;
    }

    void refusesGroupOrWorldAccess() {
        QFETCH(int, mode);
        QTemporaryDir root;
        const QString path = root.filePath(QStringLiteral("ipc"));
        QVERIFY(QDir().mkdir(path));
        QVERIFY(chmodPath(path, static_cast<mode_t>(mode)));
        QString error;
        QVERIFY(!PrivateDirectory::isPrivate(path, &error));
        QVERIFY(!error.isEmpty());
        QVERIFY(!PrivateDirectory::ensure(path));
        // Refusing never loosens or repairs the existing directory.
        QCOMPARE(modeOf(path), static_cast<mode_t>(mode));
    }

    void refusesSymlinkToPrivateDirectory() {
        QTemporaryDir root;
        const QString target = root.filePath(QStringLiteral("real"));
        QVERIFY(PrivateDirectory::ensure(target));
        const QString link = root.filePath(QStringLiteral("link"));
        QVERIFY(QFile::link(target, link));
        QVERIFY(!PrivateDirectory::isPrivate(link));
        QVERIFY(!PrivateDirectory::ensure(link));
    }

    void refusesRegularFile() {
        QTemporaryDir root;
        const QString path = root.filePath(QStringLiteral("file"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();
        QVERIFY(chmodPath(path, 0600));
        QVERIFY(!PrivateDirectory::isPrivate(path));
        QVERIFY(!PrivateDirectory::ensure(path));
    }

    void refusesMissingDirectory() {
        QTemporaryDir root;
        QVERIFY(!PrivateDirectory::isPrivate(root.filePath(QStringLiteral("missing"))));
    }

    void refusesDirectoryOfAnotherUser() {
        if (::geteuid() == 0) {
            QSKIP("running as root: no directory owned by another user is guaranteed");
        }
        // The filesystem root is owned by root on any sane system.
        QString error;
        QVERIFY(!PrivateDirectory::isPrivate(QStringLiteral("/"), &error));
        QVERIFY(!error.isEmpty());
    }

    void peerCredentialsMatchOwnUser() {
        int fds[2] = {-1, -1};
        QCOMPARE(::socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);
        QVERIFY(PrivateDirectory::isPeerSameUser(fds[0]));
        QVERIFY(PrivateDirectory::isPeerSameUser(fds[1]));
        ::close(fds[0]);
        ::close(fds[1]);
        QVERIFY(!PrivateDirectory::isPeerSameUser(-1));
    }
};

QTEST_GUILESS_MAIN(TestPrivateDirectory)
#include "tst_privatedirectory.moc"
