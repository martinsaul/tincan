// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "composition.h"
#include "core/version.h"
#include "posix_signal_watcher.h"
#include "sip/pj_version.h"

#include <QApplication>
#include <QByteArray>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>

#include <iostream>

namespace {

bool requestsCliOnlyOutput(int argc, char *argv[]) {
    for (int i = 1; i < argc; ++i) {
        const QByteArray arg(argv[i]);
        if (arg == "-v" || arg == "--version" || arg == "-h" || arg == "--help" ||
            arg == "--help-all") {
            return true;
        }
    }
    return false;
}

void ensurePlatformForHeadlessCli(int argc, char *argv[]) {
    if (requestsCliOnlyOutput(argc, argv) && qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM") &&
        qEnvironmentVariableIsEmpty("DISPLAY") && qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY")) {
        qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
    }
}

struct CliOptions {
    bool version = false;
    bool nullAudio = false;
    QString screenshotDir;
};

CliOptions parseCommandLine(const QCoreApplication &app) {
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Lightweight SIP softphone for Linux"));
    parser.addHelpOption();
    const QCommandLineOption versionOpt(
        {QStringLiteral("v"), QStringLiteral("version")},
        QStringLiteral("Display Tincan and PJSIP version information and exit."));
    const QCommandLineOption nullAudioOpt(
        QStringLiteral("null-audio"),
        QStringLiteral("Use the PJSIP null audio device instead of hardware audio."));
    QCommandLineOption screenshotOpt(
        QStringLiteral("screenshot"),
        QStringLiteral("Render main and settings windows to <dir> and exit."),
        QStringLiteral("dir"));
    screenshotOpt.setFlags(QCommandLineOption::HiddenFromHelp);

    parser.addOption(versionOpt);
    parser.addOption(nullAudioOpt);
    parser.addOption(screenshotOpt);
    parser.process(app);

    return CliOptions{parser.isSet(versionOpt), parser.isSet(nullAudioOpt),
                      parser.value(screenshotOpt)};
}

} // namespace

int main(int argc, char *argv[]) {
    ensurePlatformForHeadlessCli(argc, argv);

    QApplication app(argc, argv);
    QApplication::setQuitOnLastWindowClosed(false);
    QCoreApplication::setOrganizationName(QStringLiteral("Tincan"));
    QCoreApplication::setApplicationName(QStringLiteral("Tincan"));
    QCoreApplication::setApplicationVersion(tincan::Version::string());

    const CliOptions cli = parseCommandLine(app);
    if (cli.version) {
        std::cout << "Tincan " << tincan::Version::string().toStdString() << " (PJSIP "
                  << tincan::sip::PjVersion::string() << ")\n";
        return 0;
    }

    tincan::PosixSignalWatcher signalWatcher;
    QObject::connect(&signalWatcher, &tincan::PosixSignalWatcher::terminationRequested, &app,
                     &QCoreApplication::quit);

    tincan::Composition composition;
    QString engineError;
    if (!composition.start(cli.nullAudio, &engineError)) {
        std::cerr << "Failed to start SIP engine: " << engineError.toStdString() << '\n';
        return 1;
    }
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &app,
                     [&composition]() { composition.shutdown(); });

    if (!cli.screenshotDir.isEmpty()) {
        const bool ok = composition.saveScreenshots(cli.screenshotDir);
        composition.shutdown();
        return ok ? 0 : 1;
    }

    if (composition.shouldShowMainWindowOnStart()) {
        composition.showMainWindow();
    }

    const int rc = QApplication::exec();
    composition.shutdown();
    return rc;
}
