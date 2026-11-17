// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "sip/sip_buddy.h"

#include "sip/presence_mapper.h"

#include <pjsua-lib/pjsua.h>

#include <utility>

namespace tincan::sip {

SipBuddy::SipBuddy(QString accountId, QString uri, EventMarshaller &marshaller,
                   PresenceHandler onPresenceChanged)
    : accountId_(std::move(accountId)), uri_(std::move(uri)), marshaller_(marshaller),
      onPresenceChanged_(std::move(onPresenceChanged)) {}

SipBuddy::~SipBuddy() {
    stopSubscription();
}

void SipBuddy::startSubscription(pj::Account &account) {
    pj::BuddyConfig cfg;
    cfg.uri = uri_.toStdString();
    cfg.subscribe = true;
    cfg.subscribe_dlg_event = false;
    create(account, cfg);
}

void SipBuddy::stopSubscription() {
    if (!isValid()) {
        return;
    }
    try {
        subscribePresence(false);
    } catch (const pj::Error &) {
    }
}

const QString &SipBuddy::accountId() const { return accountId_; }

const QString &SipBuddy::uri() const { return uri_; }

void SipBuddy::onBuddyState() {
    try {
        const pj::BuddyInfo bi = getInfo();
        const PresenceInfo mapped = PresenceMapper::fromBuddyInfo(bi, uri_);
        const PresenceHandler handler = onPresenceChanged_;
        marshaller_.post([handler, mapped]() {
            if (handler) {
                handler(mapped);
            }
        });
    } catch (const pj::Error &) {
    }
}

void SipBuddy::onBuddyDlgEventState() {
    const int buddyId = getId();
    if (buddyId < 0) {
        return;
    }
    pjsua_buddy_dlg_event_info dlgInfo;
    if (pjsua_buddy_get_dlg_event_info(buddyId, &dlgInfo) != PJ_SUCCESS) {
        return;
    }
    const PresenceInfo mapped = PresenceMapper::fromDialogEventInfo(dlgInfo, uri_);
    const PresenceHandler handler = onPresenceChanged_;
    marshaller_.post([handler, mapped]() {
        if (handler) {
            handler(mapped);
        }
    });
}

} // namespace tincan::sip
