// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "core/version.h"
#include "sip/pj_version.h"
#include "ui/main_window.h"

#include <QApplication>
#include <QByteArray>
#include <QCommandLineOption>
#include <QCommandLineParser>

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

} // namespace

int main(int argc, char *argv[]) {
    ensurePlatformForHeadlessCli(argc, argv);

    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("Tincan"));
    QCoreApplication::setApplicationName(QStringLiteral("Tincan"));
    QCoreApplication::setApplicationVersion(tincan::Version::string());

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Lightweight SIP softphone for Linux"));
    parser.addHelpOption();
    const QCommandLineOption versionOption(
        {QStringLiteral("v"), QStringLiteral("version")},
        QStringLiteral("Display Tincan and PJSIP version information and exit."));
    parser.addOption(versionOption);
    parser.process(app);

    if (parser.isSet(versionOption)) {
        std::cout << "Tincan " << tincan::Version::string().toStdString() << " (PJSIP "
                  << tincan::sip::PjVersion::string() << ")\n";
        return 0;
    }

    tincan::ui::MainWindow window;
    window.show();
    return QApplication::exec();
}
