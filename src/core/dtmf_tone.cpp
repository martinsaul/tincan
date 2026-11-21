// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "core/dtmf_tone.h"

#include <array>
#include <cstddef>

namespace tincan {

namespace {

constexpr std::array<int, 4> kRowHz = {697, 770, 852, 941};
constexpr std::array<int, 4> kColumnHz = {1209, 1336, 1477, 1633};

// Row-major keypad: row i uses kRowHz[i], column j uses kColumnHz[j].
constexpr std::array<char, 16> kKeypad = {
    '1', '2', '3', 'A', //
    '4', '5', '6', 'B', //
    '7', '8', '9', 'C', //
    '*', '0', '#', 'D', //
};

} // namespace

std::optional<DtmfTonePair> dtmfTonePair(QChar key) {
    const char ch = key.toUpper().toLatin1();
    if (ch == '\0') {
        return std::nullopt;
    }
    for (std::size_t i = 0; i < kKeypad.size(); ++i) {
        if (kKeypad[i] == ch) {
            return DtmfTonePair{kRowHz[i / 4], kColumnHz[i % 4]};
        }
    }
    return std::nullopt;
}

} // namespace tincan
