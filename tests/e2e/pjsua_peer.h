// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include <QByteArray>
#include <QProcess>
#include <QString>
#include <QStringList>

namespace tincan::test {

class PjsuaPeer {
public:
    PjsuaPeer();
    ~PjsuaPeer();

    PjsuaPeer(const PjsuaPeer &) = delete;
    PjsuaPeer &operator=(const PjsuaPeer &) = delete;

    bool start(int autoAnswerCode = 200, int durationSec = 30,
               const QString &destinationUri = {});
    // Extra pjsua command-line options for the next start().
    void setExtraArgs(const QStringList &args);
    void sendInput(const QByteArray &data);
    void stop();

    int localPort() const;
    QString sipUri() const;
    QString output() const;

private:
    bool tryStartOnce(int autoAnswerCode, int durationSec, const QString &destinationUri);
    static int allocateFreeUdpPort();

    QProcess process_;
    QStringList extraArgs_;
    QByteArray capturedOutput_;
    int localPort_ = 0;
};

} // namespace tincan::test
