// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "ui/secret_fallback_prompt.h"

#include <QMessageBox>

namespace tincan::ui {

bool SecretFallbackPrompt::ask(QWidget *parent) {
    QMessageBox box(parent);
    box.setIcon(QMessageBox::Warning);
    box.setWindowTitle(tr("No password storage available"));
    box.setText(tr("Tincan could not reach a keyring (Secret Service) to store SIP "
                   "passwords."));
    box.setInformativeText(
        tr("Store passwords unencrypted in a private file (readable only by you) instead? "
           "If you choose not to, Tincan cannot remember passwords."));
    box.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    box.setDefaultButton(QMessageBox::No);
    return box.exec() == QMessageBox::Yes;
}

} // namespace tincan::ui
