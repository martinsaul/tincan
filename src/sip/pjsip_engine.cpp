// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "sip/pjsip_engine.h"

#include "core/sip_uri.h"
#include "sip/account_config_mapper.h"
#include "sip/call_registry.h"
#include "sip/event_marshaller.h"
#include "sip/sip_account.h"
#include "sip/sip_call.h"
#include "sip/sip_endpoint.h"
#include "sip/tone_player.h"

#include <QCoreApplication>
#include <QElapsedTimer>

#include <utility>

namespace tincan::sip {

namespace {

void setError(QString *error, const QString &message) {
    if (error != nullptr) {
        *error = message;
    }
}

pj::CallOpParam makeAudioCallOpParam() {
    pj::CallOpParam prm(true);
    prm.opt.audioCount = 1;
    prm.opt.videoCount = 0;
    prm.opt.textCount = 0;
    return prm;
}

} // namespace

PjsipEngine::PjsipEngine(ISipEvents &events)
    : events_(events), marshaller_(std::make_unique<EventMarshaller>()),
      endpoint_(std::make_unique<SipEndpoint>()),
      tonePlayer_(std::make_unique<TonePlayer>(*endpoint_)),
      calls_(std::make_unique<CallRegistry>()) {}

PjsipEngine::~PjsipEngine() {
    shutdown();
}

bool PjsipEngine::start(const EngineConfig &config, QString *error) {
    return endpoint_->start(config, error);
}

void PjsipEngine::shutdown() {
    if (!endpoint_->isStarted()) {
        return;
    }

    tonePlayer_->stopTone();
    endpoint_->hangupAllCalls();

    QElapsedTimer timer;
    timer.start();
    while (!calls_->isEmpty() && timer.elapsed() < 2000) {
        (void)endpoint_->handleEvents(50);
        QCoreApplication::sendPostedEvents(marshaller_.get());
    }

    endpoint_->stopWorkerThreads();
    QCoreApplication::removePostedEvents(marshaller_.get());
    calls_->clear();
    accounts_.clear();
    endpoint_->shutdown();
}

int PjsipEngine::localUdpPort() const {
    return endpoint_->localUdpPort();
}

bool PjsipEngine::addAccount(const AccountConfig &config, QString *error) {
    if (!endpoint_->isStarted()) {
        setError(error, QStringLiteral("SIP engine is not started"));
        return false;
    }
    if (config.id.trimmed().isEmpty()) {
        setError(error, QStringLiteral("Account id must not be empty"));
        return false;
    }
    if (accounts_.find(config.id) != accounts_.end()) {
        setError(error, QStringLiteral("Account already exists: %1").arg(config.id));
        return false;
    }

    try {
        if (!config.stunServer.trimmed().isEmpty()) {
            endpoint_->updateStunServer(config.stunServer);
        }
        const pj::AccountConfig pjCfg = AccountConfigMapper::map(config);
        auto regCb = [this](const RegistrationInfo &info) { events_.onRegistrationChanged(info); };
        auto incCb = [this](std::unique_ptr<SipCall> call, const CallInfo &info) {
            handleIncomingCallOnMainThread(std::move(call), info);
        };
        auto callCb = [this](const SipCall *call, const CallInfo &info) {
            handleCallStateOnMainThread(call, info);
        };

        auto account = std::make_unique<SipAccount>(config, endpoint_->pjEndpoint(), *marshaller_,
                                                    std::move(regCb), std::move(incCb),
                                                    std::move(callCb));
        const bool makeDefault = accounts_.empty();
        account->create(pjCfg, makeDefault);
        accounts_.emplace(config.id, std::move(account));
        return true;
    } catch (const pj::Error &err) {
        setError(error, QString::fromStdString(err.info()));
        return false;
    }
}

void PjsipEngine::removeAccount(const QString &accountId) {
    const auto it = accounts_.find(accountId);
    if (it == accounts_.end()) {
        return;
    }
    const bool wasDefault = it->second->isDefault();
    it->second->shutdown();
    accounts_.erase(it);

    if (wasDefault && !accounts_.empty()) {
        try {
            accounts_.begin()->second->setDefault();
        } catch (const pj::Error &) {
        }
    }
}

int PjsipEngine::makeCall(const QString &accountId, const QString &uri, QString *error) {
    if (!endpoint_->isStarted()) {
        setError(error, QStringLiteral("SIP engine is not started"));
        return -1;
    }
    const auto it = accounts_.find(accountId);
    if (it == accounts_.end()) {
        setError(error, QStringLiteral("Unknown account id: %1").arg(accountId));
        return -1;
    }

    const QString targetUri = SipUri::toSipUri(uri, it->second->config());
    if (targetUri.isEmpty()) {
        setError(error, QStringLiteral("Invalid destination URI"));
        return -1;
    }

    try {
        auto callCb = [this](const SipCall *call, const CallInfo &info) {
            handleCallStateOnMainThread(call, info);
        };
        auto call = std::make_unique<SipCall>(endpoint_->pjEndpoint(), *it->second, accountId,
                                              *marshaller_, std::move(callCb));
        call->makeCall(targetUri.toStdString(), makeAudioCallOpParam());
        const int callId = call->getId();
        calls_->insert(callId, std::move(call));
        return callId;
    } catch (const pj::Error &err) {
        setError(error, QString::fromStdString(err.info()));
        return -1;
    }
}

bool PjsipEngine::answer(int callId) {
    SipCall *call = calls_->find(callId);
    if (call == nullptr) {
        return false;
    }
    try {
        pj::CallOpParam prm = makeAudioCallOpParam();
        prm.statusCode = PJSIP_SC_OK;
        call->answer(prm);
        return true;
    } catch (const pj::Error &) {
        return false;
    }
}

bool PjsipEngine::reject(int callId, int statusCode) {
    if (statusCode < 300 || statusCode > 699) {
        return false;
    }
    SipCall *call = calls_->find(callId);
    if (call == nullptr) {
        return false;
    }
    try {
        pj::CallOpParam prm;
        prm.statusCode = static_cast<pjsip_status_code>(statusCode);
        call->hangup(prm);
        return true;
    } catch (const pj::Error &) {
        return false;
    }
}

bool PjsipEngine::hangup(int callId) {
    SipCall *call = calls_->find(callId);
    if (call == nullptr) {
        return false;
    }
    try {
        pj::CallOpParam prm;
        call->hangup(prm);
        return true;
    } catch (const pj::Error &) {
        return false;
    }
}

QList<CallInfo> PjsipEngine::calls() const {
    return calls_->snapshots();
}

QList<AudioDevice> PjsipEngine::audioDevices() const {
    return endpoint_->audioDevices();
}

bool PjsipEngine::setAudioDevices(int captureIndex, int playbackIndex) {
    return endpoint_->setAudioDevices(captureIndex, playbackIndex);
}

void PjsipEngine::setRingDevice(int playbackIndex) {
    tonePlayer_->setRingDevice(playbackIndex);
}

QList<CodecInfo> PjsipEngine::codecs() const {
    return endpoint_->codecs();
}

bool PjsipEngine::setCodecPriority(const QString &id, int priority) {
    return endpoint_->setCodecPriority(id, priority);
}

bool PjsipEngine::setEchoCanceller(bool enabled, int tailMs) {
    return endpoint_->setEchoCanceller(enabled, tailMs);
}

void PjsipEngine::startTone(Tone tone, const QString &customWav) {
    tonePlayer_->startTone(tone, customWav);
}

void PjsipEngine::stopTone() {
    tonePlayer_->stopTone();
}

void PjsipEngine::handleIncomingCallOnMainThread(std::unique_ptr<SipCall> call,
                                                 const CallInfo &info) {
    calls_->insert(info.callId, std::move(call));
    events_.onIncomingCall(info);
}

void PjsipEngine::handleCallStateOnMainThread(const SipCall *call, const CallInfo &info) {
    if (info.state == CallState::Disconnected) {
        calls_->eraseIfSame(info.callId, call);
    }
    events_.onCallChanged(info);
}

} // namespace tincan::sip
