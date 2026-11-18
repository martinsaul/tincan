// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "sip/call_recorder.h"

#include "sip/sip_call.h"
#include "sip/sip_endpoint.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <algorithm>
#include <cerrno>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace tincan::sip {

namespace {

// Recordings hold private call audio. PJSIP's WAV writer creates its file
// with the process umask, so the file is created (or tightened) here as an
// owner-only regular file first; PJSIP then truncates it and keeps the mode.
// A missing directory is created owner-only too. Symlinks are refused.
bool prepareOwnerOnlyFile(const QString &path) {
    const QString dir = QFileInfo(path).absolutePath();
    if (!QFileInfo(dir).isDir()) {
        if (!QDir().mkpath(QFileInfo(dir).absolutePath()) ||
            (::mkdir(QFile::encodeName(dir).constData(), S_IRWXU) != 0 && errno != EEXIST)) {
            return false;
        }
    }
    const int fd = ::open(QFile::encodeName(path).constData(),
                          O_WRONLY | O_CREAT | O_NOFOLLOW | O_CLOEXEC, S_IRUSR | S_IWUSR);
    if (fd < 0) {
        return false;
    }
    struct stat st {};
    const bool ok = ::fstat(fd, &st) == 0 && S_ISREG(st.st_mode) && st.st_uid == ::geteuid() &&
                    ::fchmod(fd, S_IRUSR | S_IWUSR) == 0;
    ::close(fd);
    return ok;
}

} // namespace

CallRecorder::CallRecorder(SipEndpoint &endpoint) : endpoint_(endpoint) {}

CallRecorder::~CallRecorder() {
    stopAll();
}

bool CallRecorder::start(int callId, SipCall &call, const QString &wavPath) {
    const QString trimmed = wavPath.trimmed();
    if (!endpoint_.isStarted() || trimmed.isEmpty()) {
        return false;
    }

    (void)stop(callId, &call);

    if (!prepareOwnerOnlyFile(trimmed)) {
        return false;
    }

    try {
        auto recorder = std::make_unique<pj::AudioMediaRecorder>();
        recorder->createRecorder(trimmed.toStdString());
        routeCapture(*recorder, call);

        pj::AudioMedia callAudio;
        if (call.tryGetAudioMedia(&callAudio)) {
            callAudio.startTransmit(*recorder);
        }

        recorders_[callId] = std::move(recorder);
        return true;
    } catch (const pj::Error &) {
        return false;
    }
}

bool CallRecorder::stop(int callId, SipCall *call) {
    const auto it = recorders_.find(callId);
    if (it == recorders_.end()) {
        return false;
    }

    if (endpoint_.isStarted()) {
        try {
            endpoint_.pjEndpoint().audDevManager().getCaptureDevMedia().stopTransmit(*it->second);
        } catch (const pj::Error &) {
        }
        if (call != nullptr) {
            pj::AudioMedia callAudio;
            if (call->tryGetAudioMedia(&callAudio)) {
                try {
                    callAudio.stopTransmit(*it->second);
                } catch (const pj::Error &) {
                }
            }
        }
    }

    recorders_.erase(it);
    return true;
}

void CallRecorder::stopAll() {
    if (endpoint_.isStarted()) {
        for (auto &[id, rec] : recorders_) {
            (void)id;
            try {
                endpoint_.pjEndpoint().audDevManager().getCaptureDevMedia().stopTransmit(*rec);
            } catch (const pj::Error &) {
            }
        }
    }
    recorders_.clear();
}

bool CallRecorder::isRecording(int callId) const {
    return recorders_.find(callId) != recorders_.end();
}

void CallRecorder::syncCallMedia(int callId, SipCall &call) {
    const auto it = recorders_.find(callId);
    if (it == recorders_.end() || !endpoint_.isStarted()) {
        return;
    }

    routeCapture(*it->second, call);
    pj::AudioMedia callAudio;
    if (call.tryGetAudioMedia(&callAudio)) {
        try {
            callAudio.startTransmit(*it->second);
        } catch (const pj::Error &) {
        }
    }
}

void CallRecorder::routeCapture(pj::AudioMediaRecorder &recorder, const SipCall &call) {
    // The recording follows what the far end hears from us: muted spans are
    // silent on our side instead of capturing the microphone.
    try {
        pj::AudioMedia &capture = endpoint_.pjEndpoint().audDevManager().getCaptureDevMedia();
        if (call.isMuted()) {
            capture.stopTransmit(recorder);
        } else {
            capture.startTransmit(recorder);
        }
    } catch (const pj::Error &) {
    }
}

bool CallRecorder::isCaptureRecorded(int callId) const {
    const auto it = recorders_.find(callId);
    if (it == recorders_.end() || !endpoint_.isStarted()) {
        return false;
    }
    try {
        const int recorderPort = it->second->getPortId();
        const pj::ConfPortInfo capture =
            endpoint_.pjEndpoint().audDevManager().getCaptureDevMedia().getPortInfo();
        return std::find(capture.listeners.begin(), capture.listeners.end(), recorderPort) !=
               capture.listeners.end();
    } catch (const pj::Error &) {
        return false;
    }
}

} // namespace tincan::sip
