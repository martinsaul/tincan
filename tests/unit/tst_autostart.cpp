// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "app/autostart.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

class TestAutostart : public QObject {
    Q_OBJECT

private slots:
    void writesAndRemovesEntryUnderXdgConfigHome() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString configHome = dir.filePath(QStringLiteral("config"));
        qputenv("XDG_CONFIG_HOME", configHome.toUtf8());

        tincan::Autostart autostart;
        const QString expected =
            configHome + QStringLiteral("/autostart/io.github.martinsaul.Tincan.desktop");
        QCOMPARE(autostart.filePath(), expected);
        QVERIFY(!autostart.isEnabled());

        QVERIFY(autostart.apply(true));
        QVERIFY(autostart.isEnabled());
        QFile file(expected);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QByteArray content = file.readAll();
        QVERIFY(content.startsWith("[Desktop Entry]\n"));
        QVERIFY(content.contains("\nType=Application\n"));
        QVERIFY(content.contains("\nName=Tincan\n"));
        QVERIFY(content.contains("\nExec=tincan --minimized\n"));
        QVERIFY(content.contains("\nIcon=io.github.martinsaul.Tincan\n"));
        file.close();

        QVERIFY(autostart.apply(true)); // idempotent
        QVERIFY(autostart.apply(false));
        QVERIFY(!QFile::exists(expected));
        QVERIFY(autostart.apply(false)); // removing twice is fine
    }

    void explicitDirectoryOverridesXdg() {
        QTemporaryDir dir;
        tincan::Autostart autostart(dir.path());
        QVERIFY(autostart.apply(true));
        QVERIFY(QFile::exists(dir.filePath(QStringLiteral("io.github.martinsaul.Tincan.desktop"))));
    }
};

QTEST_GUILESS_MAIN(TestAutostart)
#include "tst_autostart.moc"
