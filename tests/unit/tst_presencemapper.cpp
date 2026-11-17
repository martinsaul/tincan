// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "sip/account_config_mapper.h"
#include "sip/presence_mapper.h"

#include <QTest>

#include <cstring>

namespace {

pj_str_t makePjStr(const char *s) {
    return pj_str_t{const_cast<char *>(s), static_cast<pj_ssize_t>(std::strlen(s))};
}

} // namespace

class TestPresenceMapper : public QObject {
    Q_OBJECT

private slots:
    void mapsBuddyInfoPresenceAndRpidStates() {
        pj::BuddyInfo bi;
        bi.uri = "sip:204@pbx.example.com";

        bi.presStatus.status = PJSUA_BUDDY_STATUS_UNKNOWN;
        QCOMPARE(tincan::sip::PresenceMapper::fromBuddyInfo(bi).state,
                 tincan::PresenceState::Unknown);

        bi.presStatus.status = PJSUA_BUDDY_STATUS_OFFLINE;
        QCOMPARE(tincan::sip::PresenceMapper::fromBuddyInfo(bi).state,
                 tincan::PresenceState::Offline);

        bi.presStatus.status = PJSUA_BUDDY_STATUS_ONLINE;
        bi.presStatus.activity = PJRPID_ACTIVITY_UNKNOWN;
        bi.presStatus.statusText = "Online";
        QCOMPARE(tincan::sip::PresenceMapper::fromBuddyInfo(bi).state,
                 tincan::PresenceState::Online);

        bi.presStatus.activity = PJRPID_ACTIVITY_AWAY;
        bi.presStatus.note = "Lunch";
        QCOMPARE(tincan::sip::PresenceMapper::fromBuddyInfo(bi).state, tincan::PresenceState::Away);

        bi.presStatus.activity = PJRPID_ACTIVITY_BUSY;
        bi.presStatus.note = "Busy";
        QCOMPARE(tincan::sip::PresenceMapper::fromBuddyInfo(bi).state, tincan::PresenceState::Busy);

        bi.presStatus.activity = PJRPID_ACTIVITY_BUSY;
        bi.presStatus.note = "On the phone";
        QCOMPARE(tincan::sip::PresenceMapper::fromBuddyInfo(bi).state,
                 tincan::PresenceState::OnThePhone);

        bi.presStatus.activity = PJRPID_ACTIVITY_UNKNOWN;
        bi.presStatus.note = "Ringing";
        QCOMPARE(tincan::sip::PresenceMapper::fromBuddyInfo(bi).state,
                 tincan::PresenceState::Ringing);
    }

    void mapsDialogEventAndOwnPresenceAndPublishFlag() {
        pjsua_buddy_dlg_event_info dlg{};
        dlg.uri = makePjStr("sip:204@pbx.example.com");
        dlg.sub_state = PJSIP_EVSUB_STATE_ACTIVE;

        dlg.dialog_state = makePjStr("early");
        QCOMPARE(tincan::sip::PresenceMapper::fromDialogEventInfo(dlg).state,
                 tincan::PresenceState::Ringing);

        dlg.dialog_state = makePjStr("confirmed");
        QCOMPARE(tincan::sip::PresenceMapper::fromDialogEventInfo(dlg).state,
                 tincan::PresenceState::OnThePhone);

        dlg.dialog_state = makePjStr("terminated");
        QCOMPARE(tincan::sip::PresenceMapper::fromDialogEventInfo(dlg).state,
                 tincan::PresenceState::Online);

        const pj::PresenceStatus awayPj =
            tincan::sip::PresenceMapper::toPjPresenceStatus(tincan::PresenceState::Away, {});
        QCOMPARE(awayPj.status, PJSUA_BUDDY_STATUS_ONLINE);
        QCOMPARE(awayPj.activity, PJRPID_ACTIVITY_AWAY);

        tincan::AccountConfig cfg;
        cfg.id = QStringLiteral("acc");
        cfg.user = QStringLiteral("100");
        cfg.domain = QStringLiteral("pbx.example.com");
        cfg.publishPresence = true;
        const pj::AccountConfig mapped = tincan::sip::AccountConfigMapper::map(cfg);
        QCOMPARE(mapped.presConfig.publishEnabled, true);
    }
};

QTEST_GUILESS_MAIN(TestPresenceMapper)
#include "tst_presencemapper.moc"
