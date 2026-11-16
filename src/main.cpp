// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "core/engine_config.h"
#include "core/sip_events.h"
#include "core/version.h"
#include "sip/pj_version.h"
#include "sip/pjsip_engine.h"
#include "ui/main_window.h"

#include <QApplication>
#include <QByteArray>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>

#include <iostream>

namespace {

class NoopSipEvents : public tincan::ISipEvents {
public:
    void onRegistrationChanged(const tincan::RegistrationInfo &) override {}
    void onIncomingCall(const tincan::CallInfo &) override {}
    void onCallChanged(const tincan::CallInfo &) override {}
};

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

tincan::EngineConfig buildDefaultEngineConfig(bool nullAudio) {
    tincan::EngineConfig cfg;
    cfg.udpPort = 0;
    cfg.tcpPort = 0;
    cfg.nullAudio = nullAudio;
    cfg.userAgent = QStringLiteral("Tincan/") + tincan::Version::string();
    return cfg;
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
    const QCommandLineOption nullAudioOption(
        QStringLiteral("null-audio"),
        QStringLiteral("Use the PJSIP null audio device instead of hardware audio."));
    parser.addOption(versionOption);
    parser.addOption(nullAudioOption);
    parser.process(app);

    if (parser.isSet(versionOption)) {
        std::cout << "Tincan " << tincan::Version::string().toStdString() << " (PJSIP "
                  << tincan::sip::PjVersion::string() << ")\n";
        return 0;
    }

    NoopSipEvents sipEvents;
    tincan::sip::PjsipEngine engine(sipEvents);
    QString engineError;
    if (!engine.start(buildDefaultEngineConfig(parser.isSet(nullAudioOption)), &engineError)) {
        std::cerr << "Failed to start SIP engine: " << engineError.toStdString() << '\n';
        return 1;
    }
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &app, [&engine]() { engine.shutdown(); });

    tincan::ui::MainWindow window;
    window.show();
    const int rc = QApplication::exec();
    engine.shutdown();
    return rc;
}
