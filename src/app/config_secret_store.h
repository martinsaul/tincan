// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include "app/secret_store.h"

#include <QString>

namespace tincan {

// Stores passwords in a dedicated owner-only (0600) INI file, separate from
// the main configuration. Only used when the user agreed to it because no
// keychain backend is available.
class ConfigSecretStore : public ISecretStore {
public:
    explicit ConfigSecretStore(QString filePath);

    QString filePath() const;

    void readPassword(const QString &accountId, ReadCallback callback) override;
    void writePassword(const QString &accountId, const QString &password,
                       WriteCallback callback = {}) override;
    void deletePassword(const QString &accountId, DeleteCallback callback = {}) override;

private:
    bool ensurePrivateFile() const;
    bool store(const QString &accountId, const QString *password);

    QString filePath_;
};

} // namespace tincan
