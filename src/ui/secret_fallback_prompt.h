// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include <QCoreApplication>

class QWidget;

namespace tincan::ui {

// Asks whether passwords may be kept in a private file because no keychain
// (Secret Service) is available. Returns true when the user agrees.
class SecretFallbackPrompt {
    Q_DECLARE_TR_FUNCTIONS(tincan::ui::SecretFallbackPrompt)

public:
    static bool ask(QWidget *parent = nullptr);
};

} // namespace tincan::ui
