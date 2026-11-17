// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include <QObject>

class QSocketNotifier;

namespace tincan {

class PosixSignalWatcher : public QObject {
    Q_OBJECT

public:
    explicit PosixSignalWatcher(QObject *parent = nullptr);
    ~PosixSignalWatcher() override;

signals:
    void terminationRequested();

private:
    void onSocketReadable();

    QSocketNotifier *notifier_ = nullptr;
};

} // namespace tincan
