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

    void exposesPublicSourceRepository() {
        QCOMPARE(tincan::Version::sourceUrl(), QStringLiteral("https://github.com/martinsaul/tincan"));
    }
};

QTEST_APPLESS_MAIN(TestVersion)
#include "tst_version.moc"
