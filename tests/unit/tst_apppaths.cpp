// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "app/app_paths.h"

#include <QByteArray>
#include <QTemporaryDir>
#include <QTest>

class TestAppPaths : public QObject {
    Q_OBJECT

private slots:
    void honorsXdgDirectoriesForTincan() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString xdgConfig = tempDir.filePath(QStringLiteral("cfg"));
        const QString xdgData = tempDir.filePath(QStringLiteral("data"));
        qputenv("XDG_CONFIG_HOME", xdgConfig.toUtf8());
        qputenv("XDG_DATA_HOME", xdgData.toUtf8());

        QCOMPARE(tincan::AppPaths::configDir(), xdgConfig + QStringLiteral("/tincan"));
        QCOMPARE(tincan::AppPaths::dataDir(), xdgData + QStringLiteral("/tincan"));
    }
};

QTEST_GUILESS_MAIN(TestAppPaths)
#include "tst_apppaths.moc"
