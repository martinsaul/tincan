// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "sip/sip_log_writer.h"

#include <QIODevice>

#include <cstdio>

namespace tincan::sip {

SipLogWriter::SipLogWriter(const QString &logFilePath) {
    const QString trimmed = logFilePath.trimmed();
    if (!trimmed.isEmpty()) {
        logFile_.setFileName(trimmed);
        (void)logFile_.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
    }
}

void SipLogWriter::write(const pj::LogEntry &entry) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (logFile_.isOpen()) {
        logFile_.write(entry.msg.data(), static_cast<qint64>(entry.msg.size()));
        logFile_.flush();
    }

    if (entry.level <= 2 && !entry.msg.empty()) {
        std::fwrite(entry.msg.data(), 1, entry.msg.size(), stderr);
        if (entry.msg.back() != '\n') {
            std::fputc('\n', stderr);
        }
        std::fflush(stderr);
    }
}

} // namespace tincan::sip
