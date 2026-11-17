// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "app/command_line.h"

#include "app/uri_parser.h"
#include "core/version.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QRegularExpression>

#include <array>
#include <utility>

namespace tincan {

namespace {

using Action = CommandLine::Action;

constexpr std::array<std::pair<Action, const char *>, 7> kActionNames{{
    {Action::None, "none"},
    {Action::Dial, "dial"},
    {Action::Answer, "answer"},
    {Action::Hangup, "hangup"},
    {Action::Show, "show"},
    {Action::Quit, "quit"},
    {Action::Minimized, "minimized"},
}};

std::optional<Action> actionFromName(const QString &name) {
    for (const auto &[action, text] : kActionNames) {
        if (name == QLatin1String(text)) {
            return action;
        }
    }
    return std::nullopt;
}

struct Text {
    Q_DECLARE_TR_FUNCTIONS(tincan::CommandLine)
};

struct Options {
    QCommandLineOption version{{QStringLiteral("v"), QStringLiteral("version")},
                               Text::tr("Display Tincan and PJSIP version information and exit.")};
    QCommandLineOption nullAudio{
        QStringLiteral("null-audio"),
        Text::tr("Use the PJSIP null audio device instead of hardware audio.")};
    QCommandLineOption screenshot{QStringLiteral("screenshot"),
                                  Text::tr("Render main and settings windows to <dir> and exit."),
                                  QStringLiteral("dir")};
    QCommandLineOption answer{QStringLiteral("answer"), Text::tr("Answer the ringing call.")};
    QCommandLineOption hangup{QStringLiteral("hangup"), Text::tr("Hang up the active call.")};
    QCommandLineOption show{QStringLiteral("show"), Text::tr("Show the running Tincan window.")};
    QCommandLineOption quit{QStringLiteral("quit"), Text::tr("Quit the running Tincan instance.")};
    QCommandLineOption minimized{QStringLiteral("minimized"),
                                 Text::tr("Start hidden in the system tray.")};

    Options() { screenshot.setFlags(QCommandLineOption::HiddenFromHelp); }
};

// QCommandLineParser has no epilogue; this is appended to its help text.
QString helpEpilogue() {
    return QStringLiteral("\n%1\n  tincan sip:200@pbx.example.com\n  tincan tel:+15551234567\n"
                          "  tincan --answer\n  tincan --hangup\n\n%2\n%3\n")
        .arg(Text::tr("Examples:"),
             Text::tr("A second invocation forwards its command to the running instance."),
             Text::tr("Source code and bug reports: %1").arg(Version::sourceUrl()));
}

void setupParser(QCommandLineParser &parser, const Options &opts) {
    parser.setApplicationDescription(
        Text::tr("Tincan %1 - lightweight SIP softphone for Linux").arg(Version::string()));
    parser.addHelpOption();
    parser.addOptions({opts.version, opts.nullAudio, opts.screenshot, opts.answer, opts.hangup,
                       opts.show, opts.quit, opts.minimized});
    parser.addPositionalArgument(
        QStringLiteral("target"),
        Text::tr("Number or sip:, sips:, tel:, callto: URI to put in the dial field."), QStringLiteral("[target]"));
}

bool looksLikeUri(const QString &text) {
    static const QRegularExpression schemeRe(QStringLiteral("^[A-Za-z][A-Za-z0-9+.-]*:"));
    return schemeRe.match(text).hasMatch();
}

// Returns the dial target, or an empty string if the argument is not dialable.
QString dialTargetFromArgument(const QString &argument) {
    const QString trimmed = argument.trimmed();
    if (UriParser::hasSupportedScheme(trimmed)) {
        return UriParser::parse(trimmed);
    }
    if (trimmed.isEmpty() || looksLikeUri(trimmed) ||
        trimmed.contains(QRegularExpression(QStringLiteral("[\\s<>\"]")))) {
        return {};
    }
    return trimmed;
}

QList<Action> requestedActions(const QCommandLineParser &parser, const Options &opts) {
    QList<Action> actions;
    const std::array<std::pair<const QCommandLineOption *, Action>, 5> flags{{
        {&opts.answer, Action::Answer},
        {&opts.hangup, Action::Hangup},
        {&opts.show, Action::Show},
        {&opts.quit, Action::Quit},
        {&opts.minimized, Action::Minimized},
    }};
    for (const auto &[option, action] : flags) {
        if (parser.isSet(*option)) {
            actions.append(action);
        }
    }
    return actions;
}

bool applyPositional(const QStringList &positional, CommandLineParseResult &result) {
    if (positional.isEmpty()) {
        return true;
    }
    if (positional.size() > 1) {
        result.errorText = Text::tr("Only one dial target may be given.");
        return false;
    }
    const QString target = dialTargetFromArgument(positional.first());
    if (target.isEmpty()) {
        result.errorText = Text::tr("Cannot dial \"%1\": unsupported or malformed address.")
                               .arg(positional.first());
        return false;
    }
    result.command.action = Action::Dial;
    result.command.target = target;
    return true;
}

bool applyActions(const QCommandLineParser &parser, const Options &opts,
                  CommandLineParseResult &result) {
    const QList<Action> actions = requestedActions(parser, opts);
    const bool hasTarget = !parser.positionalArguments().isEmpty();
    if (actions.size() + (hasTarget ? 1 : 0) > 1) {
        result.errorText = Text::tr("Only one of a dial target, --answer, --hangup, --show, "
                                    "--quit or --minimized may be given.");
        return false;
    }
    if (!actions.isEmpty()) {
        result.command.action = actions.first();
        return true;
    }
    return applyPositional(parser.positionalArguments(), result);
}

} // namespace

QString commandLineActionName(CommandLine::Action action) {
    for (const auto &[candidate, text] : kActionNames) {
        if (candidate == action) {
            return QLatin1String(text);
        }
    }
    return QStringLiteral("none");
}

QByteArray CommandLine::toJson() const {
    QJsonObject obj;
    obj.insert(QStringLiteral("action"), commandLineActionName(action));
    obj.insert(QStringLiteral("target"), target);
    obj.insert(QStringLiteral("nullAudio"), nullAudio);
    obj.insert(QStringLiteral("screenshotDir"), screenshotDir);
    obj.insert(QStringLiteral("version"), version);
    return QJsonDocument(obj).toJson(QJsonDocument::Compact);
}

std::optional<CommandLine> CommandLine::fromJson(const QByteArray &json) {
    QJsonParseError error{};
    const QJsonDocument doc = QJsonDocument::fromJson(json, &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        return std::nullopt;
    }
    const QJsonObject obj = doc.object();
    const std::optional<Action> action =
        actionFromName(obj.value(QStringLiteral("action")).toString());
    if (!action) {
        return std::nullopt;
    }
    CommandLine cmd;
    cmd.action = *action;
    cmd.target = obj.value(QStringLiteral("target")).toString();
    cmd.nullAudio = obj.value(QStringLiteral("nullAudio")).toBool();
    cmd.screenshotDir = obj.value(QStringLiteral("screenshotDir")).toString();
    cmd.version = obj.value(QStringLiteral("version")).toBool();
    if (cmd.action == Action::Dial && cmd.target.isEmpty()) {
        return std::nullopt;
    }
    return cmd;
}

CommandLineParseResult parseCommandLine(const QStringList &arguments) {
    CommandLineParseResult result;
    const Options opts;
    QCommandLineParser parser;
    setupParser(parser, opts);
    result.helpText = parser.helpText() + helpEpilogue();
    if (!parser.parse(arguments)) {
        result.ok = false;
        result.errorText = parser.errorText();
        return result;
    }
    if (parser.isSet(QStringLiteral("help"))) {
        result.helpRequested = true;
        return result;
    }
    result.command.version = parser.isSet(opts.version);
    result.command.nullAudio = parser.isSet(opts.nullAudio);
    result.command.screenshotDir = parser.value(opts.screenshot);
    result.ok = applyActions(parser, opts, result);
    return result;
}

} // namespace tincan
