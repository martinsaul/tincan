// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "sip/pj_version.h"

#include <QString>
#include <QTest>

class TestPjVersion : public QObject {
    Q_OBJECT

private slots:
    void startsWithPinnedMajorMinorVersion() {
        const QString version = QString::fromStdString(tincan::sip::PjVersion::string());
        QVERIFY2(!version.isEmpty(), "pj_get_version() returned an empty string");
        QVERIFY2(version.startsWith(QStringLiteral("2.17")),
                 qPrintable(QStringLiteral("Expected version starting with 2.17, got: %1")
                                .arg(version)));
    }
};

QTEST_APPLESS_MAIN(TestPjVersion)
#include "tst_pjversion.moc"
