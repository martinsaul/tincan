// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "app/auto_answer_policy.h"

#include <QRegularExpression>
#include <QStringList>

#include <algorithm>
#include <optional>

namespace tincan {

namespace {

QString lookupHeaderCaseInsensitive(const QMap<QString, QString> &headers, const QString &name) {
    for (auto it = headers.cbegin(); it != headers.cend(); ++it) {
        if (it.key().compare(name, Qt::CaseInsensitive) == 0) {
            return it.value();
        }
    }
    return {};
}

std::optional<int> parseCallInfoAnswerAfter(const QString &value) {
    if (value.isEmpty()) {
        return std::nullopt;
    }
    static const QRegularExpression re(
        QStringLiteral("(?:^|[;,\\s])answer-after\\s*=\\s*\"?(\\d+)\"?(?:$|[;,\\s])"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = re.match(value);
    if (!match.hasMatch()) {
        return std::nullopt;
    }
    bool ok = false;
    const int seconds = match.captured(1).toInt(&ok);
    return (ok && seconds >= 0) ? std::optional<int>(seconds) : std::nullopt;
}

bool matchesAlertInfoAutoAnswer(const QString &value) {
    if (value.isEmpty()) {
        return false;
    }
    if (value.contains(QStringLiteral("intercom"), Qt::CaseInsensitive)) {
        return true;
    }
    static const QRegularExpression infoRe(
        QStringLiteral("info\\s*=\\s*\"?alert-autoanswer\"?"),
        QRegularExpression::CaseInsensitiveOption);
    return infoRe.match(value).hasMatch();
}

bool matchesAnswerModeAuto(const QString &value) {
    if (value.isEmpty()) {
        return false;
    }
    const QStringList entries = value.split(u',', Qt::SkipEmptyParts);
    for (const QString &entry : entries) {
        const qsizetype semi = entry.indexOf(u';');
        const QString modeToken = (semi >= 0 ? entry.left(semi) : entry).trimmed();
        if (modeToken.compare(QStringLiteral("Auto"), Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return false;
}

} // namespace

AutoAnswerDecision AutoAnswerPolicy::evaluate(const AutoAnswerSettings &settings,
                                              const CallInfo &call, bool hasOtherActiveCall) {
    if (hasOtherActiveCall || !call.isIncomingRinging() || settings.mode == AutoAnswerMode::Off) {
        return {false, 0};
    }

    const int defaultDelay = std::max(0, settings.delaySec);
    if (settings.mode == AutoAnswerMode::Always) {
        return {true, defaultDelay};
    }

    const QString callInfo =
        lookupHeaderCaseInsensitive(call.incomingHeaders, QStringLiteral("Call-Info"));
    if (const auto afterSec = parseCallInfoAnswerAfter(callInfo); afterSec.has_value()) {
        return {true, *afterSec};
    }

    const QString alertInfo =
        lookupHeaderCaseInsensitive(call.incomingHeaders, QStringLiteral("Alert-Info"));
    if (matchesAlertInfoAutoAnswer(alertInfo)) {
        return {true, defaultDelay};
    }

    const QString answerMode =
        lookupHeaderCaseInsensitive(call.incomingHeaders, QStringLiteral("Answer-Mode"));
    if (matchesAnswerModeAuto(answerMode)) {
        return {true, defaultDelay};
    }

    return {false, 0};
}

} // namespace tincan
