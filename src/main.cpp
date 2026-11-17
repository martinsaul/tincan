// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "app/command_line.h"
#include "app/single_instance.h"
#include "composition.h"
#include "core/version.h"
#include "posix_signal_watcher.h"
#include "sip/pj_version.h"

#include <QApplication>
#include <QByteArray>
#include <QCoreApplication>
#include <QLibraryInfo>
#include <QLocale>
#include <QTemporaryDir>
#include <QTranslator>

#include <iostream>
#include <optional>

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

void installTranslations(QTranslator &qtTranslator, QTranslator &appTranslator) {
    const QLocale locale;
    if (qtTranslator.load(locale, QStringLiteral("qtbase"), QStringLiteral("_"),
                          QLibraryInfo::path(QLibraryInfo::TranslationsPath))) {
        QCoreApplication::installTranslator(&qtTranslator);
    }
    if (appTranslator.load(locale, QStringLiteral("tincan"), QStringLiteral("_"),
                           QStringLiteral(":/i18n"))) {
        QCoreApplication::installTranslator(&appTranslator);
    }
}

// Returns an exit code when the process should stop before starting the UI.
std::optional<int> handleInformationalOptions(const tincan::CommandLineParseResult &parsed) {
    if (!parsed.ok) {
        std::cerr << parsed.errorText.toStdString() << '\n';
        return 2;
    }
    if (parsed.helpRequested) {
        std::cout << parsed.helpText.toStdString();
        return 0;
    }
    if (parsed.command.version) {
        std::cout << "Tincan " << tincan::Version::string().toStdString() << " (PJSIP "
                  << tincan::sip::PjVersion::string() << ")\n";
        return 0;
    }
    return std::nullopt;
}

int runScreenshots(const tincan::CommandLine &cmd) {
    QTemporaryDir tempDir;
    tincan::Composition composition(tempDir.filePath(QStringLiteral("tincan.ini")),
                                    QStringLiteral(":memory:"));
    QString engineError;
    if (!composition.start(cmd.nullAudio, &engineError)) {
        std::cerr << "Failed to start SIP engine: " << engineError.toStdString() << '\n';
        return 1;
    }
    const bool ok = composition.saveScreenshots(cmd.screenshotDir);
    composition.shutdown();
    return ok ? 0 : 1;
}

int runPrimary(tincan::SingleInstance &instance, const tincan::CommandLine &cmd) {
    tincan::PosixSignalWatcher signalWatcher;
    QObject::connect(&signalWatcher, &tincan::PosixSignalWatcher::terminationRequested,
                     qApp, &QCoreApplication::quit);

    tincan::Composition composition;
    QString engineError;
    if (!composition.start(cmd.nullAudio, &engineError)) {
        std::cerr << "Failed to start SIP engine: " << engineError.toStdString() << '\n';
        return 1;
    }
    composition.enableInteractivePrompts();
    QObject::connect(qApp, &QCoreApplication::aboutToQuit, qApp,
                     [&composition]() { composition.shutdown(); });
    QObject::connect(&instance, &tincan::SingleInstance::commandReceived, qApp,
                     [&composition](const tincan::CommandLine &forwarded) {
                         composition.handleForwardedCommand(forwarded);
                     });
    composition.applyStartupCommand(cmd);

    const int rc = QApplication::exec();
    composition.shutdown();
    return rc;
}

} // namespace

int main(int argc, char *argv[]) {
    ensurePlatformForHeadlessCli(argc, argv);

    QApplication app(argc, argv);
    QApplication::setQuitOnLastWindowClosed(false);
    QCoreApplication::setOrganizationName(QStringLiteral("Tincan"));
    QCoreApplication::setApplicationName(QStringLiteral("Tincan"));
    QCoreApplication::setApplicationVersion(tincan::Version::string());
    QGuiApplication::setDesktopFileName(QStringLiteral("io.github.martinsaul.Tincan"));

    QTranslator qtTranslator;
    QTranslator appTranslator;
    installTranslations(qtTranslator, appTranslator);

    const tincan::CommandLineParseResult parsed =
        tincan::parseCommandLine(QCoreApplication::arguments());
    if (const std::optional<int> rc = handleInformationalOptions(parsed)) {
        return *rc;
    }
    const tincan::CommandLine &cmd = parsed.command;
    if (!cmd.screenshotDir.isEmpty()) {
        return runScreenshots(cmd);
    }

    tincan::SingleInstance instance;
    switch (instance.start(cmd)) {
    case tincan::SingleInstance::Role::Forwarded:
        return 0;
    case tincan::SingleInstance::Role::Failed:
        std::cerr << "Tincan: " << instance.errorString().toStdString() << '\n';
        return 1;
    case tincan::SingleInstance::Role::Primary:
        break;
    }
    if (cmd.action == tincan::CommandLine::Action::Quit) {
        return 0; // nothing is running, so there is nothing to quit
    }
    return runPrimary(instance, cmd);
}
