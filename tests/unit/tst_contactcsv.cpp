// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "app/contact_csv.h"

#include <QTest>

class TestContactCsv : public QObject {
    Q_OBJECT

private slots:
    void roundTripHandlesQuotesCommasAndEmbeddedNewlines() {
        tincan::Contact c1;
        c1.name = QStringLiteral("Vance, Alice \"AJ\"");
        c1.number = QStringLiteral("+1 (555) 204-0001");
        c1.presence = true;

        tincan::Contact c2;
        c2.name = QStringLiteral("Multi\nLine\r\nName");
        c2.number = QStringLiteral("sip:bob@pbx.example.com");
        c2.presence = false;

        const QString csv = tincan::ContactCsv::exportContacts({c1, c2});
        QVERIFY(csv.startsWith(QStringLiteral("name,number,presence\r\n")));

        const auto parsed = tincan::ContactCsv::parseContacts(csv);
        QCOMPARE(parsed.imported, 2);
        QCOMPARE(parsed.skippedDuplicates, 0);
        QCOMPARE(parsed.skippedInvalid, 0);
        QCOMPARE(parsed.contacts.size(), 2);
        QCOMPARE(parsed.contacts[0].name, c1.name);
        QCOMPARE(parsed.contacts[0].number, c1.number);
        QCOMPARE(parsed.contacts[0].presence, true);
        QCOMPARE(parsed.contacts[1].name, QStringLiteral("Multi\nLine\r\nName"));
        QCOMPARE(parsed.contacts[1].number, c2.number);
        QCOMPARE(parsed.contacts[1].presence, false);
    }

    void importSkipsNormalizedDuplicatesAndReportsCounts() {
        tincan::Contact existing;
        existing.name = QStringLiteral("Existing Alice");
        existing.number = QStringLiteral("+15552040001");

        const QString csv = QStringLiteral(
            "name,number,presence\n"
            "Duplicate Existing,+1 (555) 204-0001,1\n"
            "New Bob,305,1\n"
            "Duplicate In File,(305),0\n"
            "Missing Number,,0\n"
            "New Carol,410,false\n");

        const auto res = tincan::ContactCsv::parseContacts(csv, {existing});
        QCOMPARE(res.imported, 2);
        QCOMPARE(res.skippedDuplicates, 2);
        QCOMPARE(res.skippedInvalid, 1);
        QCOMPARE(res.contacts.size(), 2);
        QCOMPARE(res.contacts[0].name, QStringLiteral("New Bob"));
        QCOMPARE(res.contacts[0].presence, true);
        QCOMPARE(res.contacts[1].name, QStringLiteral("New Carol"));
        QCOMPARE(res.contacts[1].presence, false);
    }
};

QTEST_GUILESS_MAIN(TestContactCsv)
#include "tst_contactcsv.moc"
