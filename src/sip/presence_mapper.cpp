// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "sip/presence_mapper.h"

namespace tincan::sip::PresenceMapper {

namespace {

QString pjStrToQString(const pj_str_t &str) {
    if (str.ptr == nullptr || str.slen <= 0) {
        return {};
    }
    return QString::fromUtf8(str.ptr, static_cast<qsizetype>(str.slen));
}

bool textMatchesAny(const QString &haystack, std::initializer_list<QLatin1StringView> needles) {
    for (const QLatin1StringView needle : needles) {
        if (haystack.contains(needle, Qt::CaseInsensitive)) {
            return true;
        }
    }
    return false;
}

PresenceState classifyOnlineStatus(pjrpid_activity activity, const QString &combinedText) {
    if (textMatchesAny(combinedText,
                       {QLatin1StringView("on the phone"), QLatin1StringView("on-the-phone"),
                        QLatin1StringView("in a call"), QLatin1StringView("talking")})) {
        return PresenceState::OnThePhone;
    }
    if (textMatchesAny(combinedText,
                       {QLatin1StringView("ringing"), QLatin1StringView("alerting")})) {
        return PresenceState::Ringing;
    }
    if (activity == PJRPID_ACTIVITY_AWAY ||
        textMatchesAny(combinedText, {QLatin1StringView("away"), QLatin1StringView("idle")})) {
        return PresenceState::Away;
    }
    if (activity == PJRPID_ACTIVITY_BUSY ||
        textMatchesAny(combinedText,
                       {QLatin1StringView("busy"), QLatin1StringView("dnd"),
                        QLatin1StringView("do not disturb")})) {
        return PresenceState::Busy;
    }
    return PresenceState::Online;
}

} // namespace

PresenceInfo fromBuddyInfo(const pj::BuddyInfo &info, const QString &fallbackUri) {
    PresenceInfo out;
    const QString infoUri = QString::fromStdString(info.uri).trimmed();
    out.uri = fallbackUri.trimmed().isEmpty() ? infoUri : fallbackUri.trimmed();

    const QString rpidNote = QString::fromStdString(info.presStatus.note).trimmed();
    const QString statusText = QString::fromStdString(info.presStatus.statusText).trimmed();
    out.note = !rpidNote.isEmpty() ? rpidNote : statusText;

    switch (info.presStatus.status) {
    case PJSUA_BUDDY_STATUS_UNKNOWN:
        out.state = PresenceState::Unknown;
        break;
    case PJSUA_BUDDY_STATUS_OFFLINE:
        out.state = PresenceState::Offline;
        break;
    case PJSUA_BUDDY_STATUS_ONLINE: {
        const QString combined = rpidNote + QLatin1Char(' ') + statusText;
        out.state = classifyOnlineStatus(info.presStatus.activity, combined);
        break;
    }
    }
    return out;
}

PresenceInfo fromDialogEventInfo(const pjsua_buddy_dlg_event_info &info,
                                 const QString &fallbackUri) {
    PresenceInfo out;
    const QString infoUri = pjStrToQString(info.uri).trimmed();
    out.uri = fallbackUri.trimmed().isEmpty() ? infoUri : fallbackUri.trimmed();

    const QString dlgState = pjStrToQString(info.dialog_state).trimmed().toLower();
    out.note = dlgState;

    if (dlgState == QStringLiteral("early") || dlgState == QStringLiteral("proceeding") ||
        dlgState == QStringLiteral("trying")) {
        out.state = PresenceState::Ringing;
    } else if (dlgState == QStringLiteral("confirmed")) {
        out.state = PresenceState::OnThePhone;
    } else if (dlgState == QStringLiteral("terminated")) {
        out.state = PresenceState::Online;
    } else if (info.sub_state == PJSIP_EVSUB_STATE_ACTIVE) {
        out.state = PresenceState::Online;
    } else {
        out.state = PresenceState::Unknown;
    }
    return out;
}

pj::PresenceStatus toPjPresenceStatus(PresenceState state, const QString &note) {
    pj::PresenceStatus st;
    const QString trimmedNote = note.trimmed();

    switch (state) {
    case PresenceState::Unknown:
    case PresenceState::Offline:
        st.status = PJSUA_BUDDY_STATUS_OFFLINE;
        st.activity = PJRPID_ACTIVITY_UNKNOWN;
        st.note = trimmedNote.toStdString();
        break;
    case PresenceState::Online:
        st.status = PJSUA_BUDDY_STATUS_ONLINE;
        st.activity = PJRPID_ACTIVITY_UNKNOWN;
        st.note = trimmedNote.toStdString();
        break;
    case PresenceState::Away:
        st.status = PJSUA_BUDDY_STATUS_ONLINE;
        st.activity = PJRPID_ACTIVITY_AWAY;
        st.note = (trimmedNote.isEmpty() ? QStringLiteral("Away") : trimmedNote).toStdString();
        break;
    case PresenceState::Busy:
        st.status = PJSUA_BUDDY_STATUS_ONLINE;
        st.activity = PJRPID_ACTIVITY_BUSY;
        st.note = (trimmedNote.isEmpty() ? QStringLiteral("Busy") : trimmedNote).toStdString();
        break;
    case PresenceState::Ringing:
        st.status = PJSUA_BUDDY_STATUS_ONLINE;
        st.activity = PJRPID_ACTIVITY_BUSY;
        st.note = (trimmedNote.isEmpty() ? QStringLiteral("Ringing") : trimmedNote).toStdString();
        break;
    case PresenceState::OnThePhone:
        st.status = PJSUA_BUDDY_STATUS_ONLINE;
        st.activity = PJRPID_ACTIVITY_BUSY;
        st.note =
            (trimmedNote.isEmpty() ? QStringLiteral("On the phone") : trimmedNote).toStdString();
        break;
    }
    return st;
}

} // namespace tincan::sip::PresenceMapper
