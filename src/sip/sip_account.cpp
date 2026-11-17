// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "sip/sip_account.h"

#include "core/sip_headers.h"
#include "core/sip_uri.h"
#include "sip/presence_mapper.h"

#include <pjsua-lib/pjsua.h>

#include <QByteArray>
#include <QDateTime>

#include <cstdint>
#include <utility>

namespace tincan::sip {

namespace {

RegState mapRegState(const pj::OnRegStateParam &prm) {
    if (prm.status == PJ_SUCCESS && (prm.code / 100 == 2)) {
        return prm.expiration > 0 ? RegState::Registered : RegState::Unregistered;
    }
    if (prm.code / 100 == 1) {
        return RegState::Registering;
    }
    return RegState::Failed;
}

} // namespace

SipAccount::SipAccount(AccountConfig config, pj::Endpoint &endpoint, EventMarshaller &marshaller,
                       AccountCallbacks accountCallbacks, SipCall::Callbacks callCallbacks)
    : config_(std::move(config)), endpoint_(endpoint), marshaller_(marshaller),
      accountCallbacks_(std::move(accountCallbacks)), callCallbacks_(std::move(callCallbacks)) {}

SipAccount::~SipAccount() {
    shutdown();
}

void SipAccount::onRegStarted(pj::OnRegStartedParam &prm) {
    if (!prm.renew) {
        return;
    }
    RegistrationInfo info{config_.id, RegState::Registering, 100, QStringLiteral("Registering")};
    const auto handler = accountCallbacks_.onRegChanged;
    marshaller_.post([handler, info]() {
        if (handler) {
            handler(info);
        }
    });
}

void SipAccount::onRegState(pj::OnRegStateParam &prm) {
    RegistrationInfo info;
    info.accountId = config_.id;
    info.state = mapRegState(prm);
    info.statusCode = static_cast<int>(prm.code);
    info.reason = QString::fromStdString(prm.reason);

    const auto handler = accountCallbacks_.onRegChanged;
    marshaller_.post([handler, info]() {
        if (handler) {
            handler(info);
        }
    });
}

void SipAccount::onIncomingCall(pj::OnIncomingCallParam &prm) {
    try {
        auto call = std::make_unique<SipCall>(endpoint_, *this, config_.id, marshaller_,
                                              callCallbacks_, prm.callId);
        call->setIncomingHeaders(
            SipHeaders::parseIncomingHeaders(QString::fromStdString(prm.rdata.wholeMsg)));

        CallInfo incomingInfo = call->snapshot();
        incomingInfo.direction = CallDirection::Incoming;
        incomingInfo.state = CallState::Incoming;

        // Send 180 before handing ownership to the main thread: once posted,
        // the main thread may answer, reject, redirect or delete the call at any time.
        pj::CallOpParam ringPrm;
        ringPrm.statusCode = PJSIP_SC_RINGING;
        call->answer(ringPrm);

        auto holder = std::make_shared<std::unique_ptr<SipCall>>(std::move(call));
        const auto handler = accountCallbacks_.onIncomingCall;
        marshaller_.post([handler, holder, incomingInfo]() {
            if (handler && *holder != nullptr) {
                handler(std::move(*holder), incomingInfo);
            }
        });
    } catch (const pj::Error &) {
    }
}

void SipAccount::onInstantMessage(pj::OnInstantMessageParam &prm) {
    const QString rawFrom =
        QString::fromUtf8(prm.fromUri.data(), static_cast<qsizetype>(prm.fromUri.size()));
    const auto [fromUri, fromName] = SipUri::splitNameAddr(rawFrom);

    MessageInfo info;
    info.accountId = config_.id;
    info.fromUri = fromUri;
    info.fromName = fromName;
    info.body = QString::fromUtf8(prm.msgBody.data(), static_cast<qsizetype>(prm.msgBody.size()));
    info.contentType = QString::fromStdString(prm.contentType);
    info.receivedAt = QDateTime::currentDateTimeUtc();

    const auto handler = accountCallbacks_.onIncomingMessage;
    marshaller_.post([handler, info]() {
        if (handler) {
            handler(info);
        }
    });
}

void SipAccount::onInstantMessageStatus(pj::OnInstantMessageStatusParam &prm) {
    const auto token = static_cast<quint64>(reinterpret_cast<std::uintptr_t>(prm.userData));
    const int statusCode = static_cast<int>(prm.code);
    const QString reason = QString::fromStdString(prm.reason);

    const auto handler = accountCallbacks_.onMessageStatus;
    marshaller_.post([handler, token, statusCode, reason]() {
        if (handler) {
            handler(token, statusCode, reason);
        }
    });
}

bool SipAccount::setOwnPresence(PresenceState state, const QString &note) {
    if (!isValid()) {
        return false;
    }
    try {
        setOnlineStatus(PresenceMapper::toPjPresenceStatus(state, note));
        return true;
    } catch (const pj::Error &) {
        return false;
    }
}

bool SipAccount::sendMessage(const QString &targetUri, const QString &body, quint64 token) {
    if (!isValid() || targetUri.trimmed().isEmpty()) {
        return false;
    }
    const QByteArray toUtf8 = targetUri.toUtf8();
    const QByteArray mimeUtf8 = QByteArrayLiteral("text/plain");
    const QByteArray bodyUtf8 = body.toUtf8();

    pj_str_t pjTo{const_cast<char *>(toUtf8.constData()), static_cast<pj_ssize_t>(toUtf8.size())};
    pj_str_t pjMime{const_cast<char *>(mimeUtf8.constData()),
                    static_cast<pj_ssize_t>(mimeUtf8.size())};
    pj_str_t pjBody{const_cast<char *>(bodyUtf8.constData()),
                    static_cast<pj_ssize_t>(bodyUtf8.size())};
    auto *userData = reinterpret_cast<void *>(static_cast<std::uintptr_t>(token));

    return pjsua_im_send(getId(), &pjTo, &pjMime, &pjBody, nullptr, userData) == PJ_SUCCESS;
}

const AccountConfig &SipAccount::config() const {
    return config_;
}

} // namespace tincan::sip
