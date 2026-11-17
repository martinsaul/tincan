// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include "app/settings_store.h"
#include "core/call_info.h"
#include "core/sip_engine.h"
#include "core/tone.h"

#include <QList>
#include <QString>

#include <optional>

namespace tincan {

class CallToneController {
public:
    CallToneController(ISipEngine &engine, SettingsStore &settings);

    void update(const QList<CallInfo> &calls);

private:
    ISipEngine &engine_;
    SettingsStore &settings_;
    std::optional<Tone> activeTone_;
    QString activeToneWav_;
};

} // namespace tincan
