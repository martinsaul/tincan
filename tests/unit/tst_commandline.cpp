// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "app/command_line.h"

#include <QTest>

using tincan::CommandLine;
using Action = tincan::CommandLine::Action;

namespace {

tincan::CommandLineParseResult parseArgs(QStringList args) {
    args.prepend(QStringLiteral("tincan"));
    return tincan::parseCommandLine(args);
}

} // namespace

class TestCommandLine : public QObject {
    Q_OBJECT

private slots:
    void noArgumentsMeansNoAction() {
        const auto result = parseArgs({});
        QVERIFY(result.ok);
        QCOMPARE(result.command, CommandLine{});
    }

    void parsesDialTargets_data() {
        QTest::addColumn<QString>("argument");
        QTest::addColumn<QString>("target");
        QTest::newRow("bare number") << "100" << "100";
        QTest::newRow("bare formatted") << "+1-206-555-0100" << "+1-206-555-0100";
        QTest::newRow("sip") << "sip:100@pbx" << "sip:100@pbx";
        QTest::newRow("sips") << "sips:100@pbx" << "sips:100@pbx";
        QTest::newRow("tel") << "tel:+1-206-555-0100;phone-context=x" << "+12065550100";
        QTest::newRow("callto") << "callto:alice" << "alice";
        QTest::newRow("callto slashes") << "callto://alice" << "alice";
    }

    void parsesDialTargets() {
        QFETCH(QString, argument);
        QFETCH(QString, target);
        const auto result = parseArgs({argument});
        QVERIFY2(result.ok, qPrintable(result.errorText));
        QCOMPARE(result.command.action, Action::Dial);
        QCOMPARE(result.command.target, target);
    }

    void parsesActionFlags_data() {
        QTest::addColumn<QString>("flag");
        QTest::addColumn<Action>("action");
        QTest::newRow("answer") << "--answer" << Action::Answer;
        QTest::newRow("hangup") << "--hangup" << Action::Hangup;
        QTest::newRow("show") << "--show" << Action::Show;
        QTest::newRow("quit") << "--quit" << Action::Quit;
        QTest::newRow("minimized") << "--minimized" << Action::Minimized;
    }

    void parsesActionFlags() {
        QFETCH(QString, flag);
        QFETCH(Action, action);
        const auto result = parseArgs({flag});
        QVERIFY(result.ok);
        QCOMPARE(result.command.action, action);
        QVERIFY(result.command.target.isEmpty());
    }

    void parsesExistingOptions() {
        const auto result = parseArgs({QStringLiteral("--null-audio"), QStringLiteral("-v"),
                                       QStringLiteral("--screenshot"), QStringLiteral("/tmp/x")});
        QVERIFY(result.ok);
        QVERIFY(result.command.nullAudio);
        QVERIFY(result.command.version);
        QCOMPARE(result.command.screenshotDir, QStringLiteral("/tmp/x"));
        QCOMPARE(result.command.action, Action::None);
    }

    void helpIsReportedNotExecuted() {
        const auto result = parseArgs({QStringLiteral("--help")});
        QVERIFY(result.ok);
        QVERIFY(result.helpRequested);
        QVERIFY(result.helpText.contains(QStringLiteral("--answer")));
        QVERIFY(!result.helpText.contains(QStringLiteral("--screenshot")));
        QVERIFY(result.helpText.contains(QStringLiteral("Examples:")));
        QVERIFY(result.helpText.contains(QStringLiteral("https://github.com/martinsaul/tincan")));
    }

    void rejectsBadInput_data() {
        QTest::addColumn<QStringList>("args");
        QTest::newRow("unknown option") << QStringList{QStringLiteral("--bogus")};
        QTest::newRow("unknown scheme") << QStringList{QStringLiteral("http://example.com")};
        QTest::newRow("malformed sip") << QStringList{QStringLiteral("sip:a@b@c")};
        QTest::newRow("two targets") << QStringList{QStringLiteral("100"), QStringLiteral("200")};
        QTest::newRow("two actions")
            << QStringList{QStringLiteral("--answer"), QStringLiteral("--hangup")};
        QTest::newRow("action and target")
            << QStringList{QStringLiteral("--quit"), QStringLiteral("100")};
        QTest::newRow("whitespace target") << QStringList{QStringLiteral("10 0")};
    }

    void rejectsBadInput() {
        QFETCH(QStringList, args);
        const auto result = parseArgs(args);
        QVERIFY(!result.ok);
        QVERIFY(!result.errorText.isEmpty());
    }

    void jsonRoundTrip() {
        CommandLine cmd;
        cmd.action = Action::Dial;
        cmd.target = QStringLiteral("sip:123@host;transport=tls");
        cmd.nullAudio = true;
        const auto decoded = CommandLine::fromJson(cmd.toJson());
        QVERIFY(decoded.has_value());
        QCOMPARE(*decoded, cmd);
    }

    void jsonRejectsGarbage() {
        QVERIFY(!CommandLine::fromJson(QByteArrayLiteral("not json")).has_value());
        QVERIFY(!CommandLine::fromJson(QByteArrayLiteral("[]")).has_value());
        QVERIFY(!CommandLine::fromJson(QByteArrayLiteral(R"({"action":"explode"})")).has_value());
        QVERIFY(!CommandLine::fromJson(QByteArrayLiteral(R"({"action":"dial"})")).has_value());
    }
};

QTEST_GUILESS_MAIN(TestCommandLine)
#include "tst_commandline.moc"
