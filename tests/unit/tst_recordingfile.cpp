// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "app/recording_file.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>

#include <sys/stat.h>

namespace {

int modeOf(const QString &path) {
    struct stat st {};
    return ::lstat(QFile::encodeName(path).constData(), &st) == 0 ? int(st.st_mode & 07777) : -1;
}

const QDateTime kStamp = QDateTime(QDate(2026, 11, 18), QTime(9, 30, 0));

} // namespace

class TestRecordingFile : public QObject {
    Q_OBJECT

private slots:
    void createsOwnerOnlyFileInNewOwnerOnlyDirectory() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString dir = tmp.filePath(QStringLiteral("a/Tincan"));
        const QString path =
            tincan::RecordingFile::create(dir, QStringLiteral("sip:alice@pbx.example"), kStamp);
        QCOMPARE(path, dir + QStringLiteral("/20261118-093000-alice.wav"));
        QCOMPARE(modeOf(path), 0600);
        QCOMPARE(modeOf(dir), 0700);
        QCOMPARE(QFileInfo(path).size(), qint64(0));
    }

    void leavesExistingDirectoryModeAlone() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        QVERIFY(::chmod(QFile::encodeName(tmp.path()).constData(), 0750) == 0);
        const QString path =
            tincan::RecordingFile::create(tmp.path(), QStringLiteral("100"), kStamp);
        QVERIFY(!path.isEmpty());
        QCOMPARE(modeOf(tmp.path()), 0750);
        QCOMPARE(modeOf(path), 0600);
    }

    void neverOverwritesOrFollowsSymlinks() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString target = tmp.filePath(QStringLiteral("victim"));
        QFile victim(target);
        QVERIFY(victim.open(QIODevice::WriteOnly));
        victim.write("keep");
        victim.close();
        const QString first = tmp.filePath(QStringLiteral("20261118-093000-bob.wav"));
        QVERIFY(QFile::link(target, first));

        const QString second =
            tincan::RecordingFile::create(tmp.path(), QStringLiteral("bob"), kStamp);
        QCOMPARE(second, tmp.filePath(QStringLiteral("20261118-093000-bob-2.wav")));
        const QString third =
            tincan::RecordingFile::create(tmp.path(), QStringLiteral("bob"), kStamp);
        QCOMPARE(third, tmp.filePath(QStringLiteral("20261118-093000-bob-3.wav")));
        QVERIFY(victim.open(QIODevice::ReadOnly));
        QCOMPARE(victim.readAll(), QByteArray("keep"));
    }

    void hostileRemoteNamesStayInsideTheDirectory() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QStringList hostile = {
            QStringLiteral("sip:../../etc/passwd@h"), QStringLiteral("\"x\" <sip:..%2f..@h>"),
            QStringLiteral("/abs/path"), QStringLiteral(".."), QString(500, u'a')};
        for (const QString &remote : hostile) {
            const QString path = tincan::RecordingFile::create(tmp.path(), remote, kStamp);
            QVERIFY2(!path.isEmpty(), qPrintable(remote));
            QCOMPARE(QFileInfo(path).absolutePath(), QDir(tmp.path()).absolutePath());
            QVERIFY(QFileInfo(path).fileName().size() < 100);
        }
    }

    void discardRemovesOnlyEmptyReservations() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString empty =
            tincan::RecordingFile::create(tmp.path(), QStringLiteral("c"), kStamp);
        tincan::RecordingFile::discardIfEmpty(empty);
        QVERIFY(!QFileInfo::exists(empty));

        const QString full = tincan::RecordingFile::create(tmp.path(), QStringLiteral("c"), kStamp);
        QFile f(full);
        QVERIFY(f.open(QIODevice::Append));
        f.write("RIFF");
        f.close();
        tincan::RecordingFile::discardIfEmpty(full);
        QVERIFY(QFileInfo::exists(full));
    }
};

QTEST_GUILESS_MAIN(TestRecordingFile)
#include "tst_recordingfile.moc"
