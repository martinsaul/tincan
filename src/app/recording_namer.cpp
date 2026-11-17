// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "app/recording_namer.h"

#include <QDir>

namespace tincan::RecordingNamer {

namespace {

bool isSafeFileChar(QChar ch) {
    const ushort u = ch.unicode();
    return (u >= u'0' && u <= u'9') || (u >= u'A' && u <= u'Z') || (u >= u'a' && u <= u'z') ||
           ch == u'_' || ch == u'-' || ch == u'+' || ch == u'.';
}

QString extractRemoteStem(const QString &remoteUri) {
    QString s = remoteUri.trimmed();
    const qsizetype lt = s.indexOf(u'<');
    const qsizetype gt = s.lastIndexOf(u'>');
    if (lt >= 0 && gt > lt) {
        s = s.mid(lt + 1, gt - lt - 1).trimmed();
    }
    if (s.startsWith(QStringLiteral("sips:"), Qt::CaseInsensitive)) {
        s.remove(0, 5);
    } else if (s.startsWith(QStringLiteral("sip:"), Qt::CaseInsensitive)) {
        s.remove(0, 4);
    }
    const qsizetype semi = s.indexOf(u';');
    if (semi >= 0) {
        s = s.left(semi);
    }
    const qsizetype at = s.indexOf(u'@');
    if (at > 0) {
        s = s.left(at);
    } else {
        if (at == 0) {
            s.remove(0, 1);
        }
        const qsizetype colon = s.indexOf(u':');
        if (colon > 0) {
            s = s.left(colon);
        }
    }

    QString sanitized;
    sanitized.reserve(s.size());
    for (const QChar ch : s) {
        sanitized.append(isSafeFileChar(ch) ? ch : u'_');
    }
    while (!sanitized.isEmpty() && sanitized.front() == u'.') {
        sanitized.remove(0, 1);
    }
    return sanitized.isEmpty() ? QStringLiteral("unknown") : sanitized;
}

} // namespace

QString buildWavPath(const QString &recordingsDir, const QString &remoteUri,
                     const QDateTime &timestamp) {
    const QString stamp = timestamp.toString(QStringLiteral("yyyyMMdd-HHmmss"));
    const QString remote = extractRemoteStem(remoteUri);
    const QString fileName = QStringLiteral("%1-%2.wav").arg(stamp, remote);
    return QDir(recordingsDir).filePath(fileName);
}

} // namespace tincan::RecordingNamer
