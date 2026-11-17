// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <optional>

namespace tincan {

// A parsed Tincan command line. Pure value type; it is also the payload a
// second instance forwards to the primary one (see SingleInstance).
struct CommandLine {
    enum class Action { None, Dial, Answer, Hangup, Show, Quit, Minimized };

    Action action = Action::None;
    QString target;
    bool nullAudio = false;
    QString screenshotDir;
    bool version = false;

    QByteArray toJson() const;
    static std::optional<CommandLine> fromJson(const QByteArray &json);

    friend bool operator==(const CommandLine &, const CommandLine &) = default;
};

struct CommandLineParseResult {
    CommandLine command;
    bool ok = true;
    bool helpRequested = false;
    QString errorText;
    QString helpText;
};

// Parses argv (including argv[0]) without exiting the process.
CommandLineParseResult parseCommandLine(const QStringList &arguments);

QString commandLineActionName(CommandLine::Action action);

} // namespace tincan
