// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "posix_signal_watcher.h"

#include <QSocketNotifier>

#include <csignal>
#include <sys/socket.h>
#include <unistd.h>

namespace tincan {

namespace {

int gSignalPipeFds[2] = {-1, -1};

void handlePosixSignal(int) {
    if (gSignalPipeFds[0] >= 0) {
        const char byte = 1;
        const ssize_t unused = ::write(gSignalPipeFds[0], &byte, sizeof(byte));
        (void)unused;
    }
}

} // namespace

PosixSignalWatcher::PosixSignalWatcher(QObject *parent) : QObject(parent) {
    if (::socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, gSignalPipeFds) != 0) {
        return;
    }

    notifier_ = new QSocketNotifier(gSignalPipeFds[1], QSocketNotifier::Read, this);
    connect(notifier_, &QSocketNotifier::activated, this,
            [this](QSocketDescriptor) { onSocketReadable(); });

    struct sigaction sa{};
    sa.sa_handler = handlePosixSignal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    (void)::sigaction(SIGTERM, &sa, nullptr);
    (void)::sigaction(SIGINT, &sa, nullptr);
}

PosixSignalWatcher::~PosixSignalWatcher() {
    delete notifier_;
    notifier_ = nullptr;
    if (gSignalPipeFds[0] >= 0) {
        ::close(gSignalPipeFds[0]);
        gSignalPipeFds[0] = -1;
    }
    if (gSignalPipeFds[1] >= 0) {
        ::close(gSignalPipeFds[1]);
        gSignalPipeFds[1] = -1;
    }
}

void PosixSignalWatcher::onSocketReadable() {
    notifier_->setEnabled(false);
    char buf[16];
    const ssize_t unused = ::read(gSignalPipeFds[1], buf, sizeof(buf));
    (void)unused;
    emit terminationRequested();
    notifier_->setEnabled(true);
}

} // namespace tincan
