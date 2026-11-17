// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include "core/call_info.h"
#include "core/dtmf_method.h"
#include "sip/event_marshaller.h"

#include <QDateTime>
#include <QMap>
#include <QString>

#include <pjsua2/call.hpp>
#include <pjsua2/endpoint.hpp>
#include <pjsua2/media.hpp>

#include <atomic>
#include <functional>
#include <memory>

namespace tincan::sip {

class SipCall : public pj::Call {
public:
    // The SipCall pointer is an identity token only; it must never be dereferenced
    // by the handler (the object may already be gone).
    struct Callbacks {
        std::function<void(const SipCall *, const CallInfo &)> onStateChanged;
        std::function<void(const SipCall *, const CallInfo &)> onMediaChanged;
        std::function<void(const SipCall *, const CallInfo &)> onTransferStatus;
        std::function<void(std::unique_ptr<SipCall>, const CallInfo &)> onChildCreated;
    };

    SipCall(pj::Endpoint &endpoint, pj::Account &account, QString accountId,
            EventMarshaller &marshaller, Callbacks callbacks, int callId = PJSUA_INVALID_ID);
    ~SipCall() override;

    void onCallState(pj::OnCallStateParam &prm) override;
    void onCallMediaState(pj::OnCallMediaStateParam &prm) override;
    void onCallTransferStatus(pj::OnCallTransferStatusParam &prm) override;
    void onCallTransferRequest(pj::OnCallTransferRequestParam &prm) override;
    void onCallReplaceRequest(pj::OnCallReplaceRequestParam &prm) override;
    void onCallReplaced(pj::OnCallReplacedParam &prm) override;
    void onCreateMediaTransport(pj::OnCreateMediaTransportParam &prm) override;

    bool setHoldState(bool hold);
    bool setMuted(bool mute);
    bool sendDtmfDigits(const QString &digits, DtmfMethod method);
    bool setVolume(float rxLevel, float txLevel);
    void stopInbandDtmf();
    bool tryGetAudioMedia(pj::AudioMedia *outMedia) const;

    void setIncomingHeaders(QMap<QString, QString> headers);
    const QString &accountId() const;
    CallInfo snapshot() const;

private:
    QDateTime connectedTimestamp() const;
    SipCall *spawnPendingChild();
    void adoptSelfIfNeeded();
    bool sendInbandDtmf(const QString &digits);
    void applyMediaLevels(pj::AudioMedia &callAudio) const;

    pj::Endpoint &endpoint_;
    pj::Account &account_;
    QString accountId_;
    EventMarshaller &marshaller_;
    Callbacks callbacks_;
    QMap<QString, QString> incomingHeaders_;
    std::atomic<qint64> connectedAtMs_{0};
    std::atomic<bool> muted_{false};
    std::atomic<int> transferStatus_{0};
    std::atomic<float> rxLevel_{1.0f};
    std::atomic<float> txLevel_{1.0f};
    std::unique_ptr<pj::ToneGenerator> inbandDtmfGen_;
    std::shared_ptr<std::unique_ptr<SipCall>> pendingChild_;
    std::weak_ptr<std::unique_ptr<SipCall>> selfHolder_;
};

} // namespace tincan::sip
