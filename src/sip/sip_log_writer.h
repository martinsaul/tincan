// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include <QFile>
#include <QString>

#include <pjsua2/endpoint.hpp>

#include <mutex>

namespace tincan::sip {

class SipLogWriter : public pj::LogWriter {
public:
    explicit SipLogWriter(const QString &logFilePath);
    ~SipLogWriter() override = default;

    void write(const pj::LogEntry &entry) override;

private:
    std::mutex mutex_;
    QFile logFile_;
};

} // namespace tincan::sip
