// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "sip/pjsip_engine.h"

#include "core/sip_uri.h"
#include "sip/account_config_mapper.h"
#include "sip/buddy_registry.h"
#include "sip/call_controller.h"
#include "sip/event_marshaller.h"
#include "sip/keypad_tone_player.h"
#include "sip/sip_account.h"
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

} // namespace

PjsipEngine::PjsipEngine(ISipEvents &events)
    : events_(events), marshaller_(std::make_unique<EventMarshaller>()),
      endpoint_(std::make_unique<SipEndpoint>()),
      tonePlayer_(std::make_unique<TonePlayer>(*endpoint_)),
      keypadTones_(std::make_unique<KeypadTonePlayer>(*endpoint_)),
      callController_(std::make_unique<CallController>(
          events_, *endpoint_, *marshaller_,
          [this](const QString &accId, const QString &uri) {
              return resolveUriForAccount(accId, uri);
          })),
      buddyRegistry_(std::make_unique<BuddyRegistry>(events_, *marshaller_)) {}

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

    callController_->stopAllMedia();
    tonePlayer_->stopTone();
    keypadTones_->release();
    endpoint_->hangupAllCalls();

    QElapsedTimer timer;
    timer.start();
    while (!callController_->isEmpty() && timer.elapsed() < 2000) {
        (void)endpoint_->handleEvents(50);
        QCoreApplication::sendPostedEvents(marshaller_.get());
    }

    endpoint_->stopWorkerThreads();
    QCoreApplication::removePostedEvents(marshaller_.get());
    callController_->clearAll();
    buddyRegistry_->clear();
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
        SipAccount::AccountCallbacks accCbs{
            [this](const RegistrationInfo &info) { events_.onRegistrationChanged(info); },
            [this](std::unique_ptr<SipCall> call, const CallInfo &info) {
                callController_->adoptIncomingCall(std::move(call), info);
            },
            [this](const MessageInfo &info) { events_.onIncomingMessage(info); },
            [this](quint64 token, int code, const QString &reason) {
                events_.onMessageStatus(token, code, reason);
            },
        };

        auto account = std::make_unique<SipAccount>(config, endpoint_->pjEndpoint(), *marshaller_,
                                                    std::move(accCbs),
                                                    callController_->makeCallCallbacks());
        account->create(pjCfg, accounts_.empty());
        (void)account->setOwnPresence(PresenceState::Online, {});
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
    buddyRegistry_->removeForAccount(accountId);
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
    return callController_->makeCall(*it->second, accountId, targetUri, error);
}

bool PjsipEngine::answer(int callId) { return callController_->answer(callId); }
bool PjsipEngine::reject(int callId, int statusCode) {
    return callController_->reject(callId, statusCode);
}
bool PjsipEngine::redirect(int callId, const QString &uri) {
    return callController_->redirect(callId, uri);
}
bool PjsipEngine::hangup(int callId) { return callController_->hangup(callId); }
bool PjsipEngine::setHold(int callId, bool hold) { return callController_->setHold(callId, hold); }
bool PjsipEngine::setMute(int callId, bool mute) { return callController_->setMute(callId, mute); }
bool PjsipEngine::sendDtmf(int callId, const QString &digits, DtmfMethod method) {
    return callController_->sendDtmf(callId, digits, method);
}
bool PjsipEngine::transferBlind(int callId, const QString &uri) {
    return callController_->transferBlind(callId, uri);
}
bool PjsipEngine::transferAttended(int callId, int targetCallId) {
    return callController_->transferAttended(callId, targetCallId);
}
bool PjsipEngine::conference(const QList<int> &callIds) {
    return callController_->conference(callIds);
}
bool PjsipEngine::unconference(int callId) { return callController_->unconference(callId); }
bool PjsipEngine::startRecording(int callId, const QString &wavPath) {
    return callController_->startRecording(callId, wavPath);
}
void PjsipEngine::stopRecording(int callId) { callController_->stopRecording(callId); }
bool PjsipEngine::setCallVolume(int callId, float rxLevel, float txLevel) {
    return callController_->setCallVolume(callId, rxLevel, txLevel);
}
QList<CallInfo> PjsipEngine::calls() const { return callController_->calls(); }

QList<AudioDevice> PjsipEngine::audioDevices() const { return endpoint_->audioDevices(); }
bool PjsipEngine::setAudioDevices(int captureIndex, int playbackIndex) {
    return endpoint_->setAudioDevices(captureIndex, playbackIndex);
}
void PjsipEngine::setRingDevice(int playbackIndex) { tonePlayer_->setRingDevice(playbackIndex); }
QList<CodecInfo> PjsipEngine::codecs() const { return endpoint_->codecs(); }
bool PjsipEngine::setCodecPriority(const QString &id, int priority) {
    return endpoint_->setCodecPriority(id, priority);
}
bool PjsipEngine::setEchoCanceller(bool enabled, int tailMs) {
    return endpoint_->setEchoCanceller(enabled, tailMs);
}
void PjsipEngine::startTone(Tone tone, const QString &customWav) {
    tonePlayer_->startTone(tone, customWav);
}
void PjsipEngine::stopTone() { tonePlayer_->stopTone(); }
void PjsipEngine::playKeypadTone(QChar digit) { keypadTones_->play(digit); }
int PjsipEngine::keypadTonePortId() const { return keypadTones_->portId(); }

bool PjsipEngine::subscribePresence(const QString &accountId, const QString &uri) {
    if (!endpoint_->isStarted()) {
        return false;
    }
    const auto it = accounts_.find(accountId);
    if (it == accounts_.end()) {
        return false;
    }
    const QString targetUri = SipUri::toSipUri(uri, it->second->config());
    if (targetUri.isEmpty()) {
        return false;
    }
    return buddyRegistry_->subscribe(*it->second, accountId, targetUri);
}

void PjsipEngine::unsubscribePresence(const QString &uri) {
    buddyRegistry_->unsubscribe(uri);
}

bool PjsipEngine::setOwnPresence(const QString &accountId, PresenceState state,
                                 const QString &note) {
    if (!endpoint_->isStarted()) {
        return false;
    }
    const auto it = accounts_.find(accountId);
    if (it == accounts_.end()) {
        return false;
    }
    return it->second->setOwnPresence(state, note);
}

bool PjsipEngine::sendMessage(const QString &accountId, const QString &uri, const QString &body,
                              quint64 token) {
    if (!endpoint_->isStarted()) {
        return false;
    }
    const auto it = accounts_.find(accountId);
    if (it == accounts_.end()) {
        return false;
    }
    const QString targetUri = SipUri::toSipUri(uri, it->second->config());
    if (targetUri.isEmpty()) {
        return false;
    }
    return it->second->sendMessage(targetUri, body, token);
}

QString PjsipEngine::resolveUriForAccount(const QString &accountId, const QString &uri) const {
    const auto it = accounts_.find(accountId);
    if (it != accounts_.end()) {
        return SipUri::toSipUri(uri, it->second->config());
    }
    return SipUri::toSipUri(uri, AccountConfig{});
}

} // namespace tincan::sip
