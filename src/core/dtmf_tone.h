// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include <QChar>

#include <optional>

namespace tincan {

// The two frequencies of a DTMF key: one from the low (row) group and one
// from the high (column) group, as laid out on a 4x4 telephone keypad.
struct DtmfTonePair {
    int lowHz = 0;
    int highHz = 0;

    friend bool operator==(const DtmfTonePair &, const DtmfTonePair &) = default;
};

// Length of one local keypad feedback burst.
inline constexpr int kKeypadToneMs = 120;

// 0-9, *, # and A-D (either case); anything else has no tone.
std::optional<DtmfTonePair> dtmfTonePair(QChar key);

} // namespace tincan
