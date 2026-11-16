// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "core/version.h"

#include <QTest>

class TestVersion : public QObject {
    Q_OBJECT

private slots:
    void returnsConfiguredProjectVersion() {
        QCOMPARE(tincan::Version::string(), QStringLiteral("0.1.0"));
    }
};

QTEST_APPLESS_MAIN(TestVersion)
#include "tst_version.moc"
