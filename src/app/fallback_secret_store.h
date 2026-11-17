// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include "app/secret_store.h"
#include "app/settings_store.h"

#include <QList>

#include <functional>

namespace tincan {

// Uses the keychain store normally. When it reports that no backend exists,
// asks the user once (through an injected callback) whether passwords may be
// kept in the private config-file store instead, and remembers the answer.
class FallbackSecretStore : public ISecretStore {
public:
    // Returns true when the user allows storing passwords in a file.
    using ConsentPrompt = std::function<bool()>;

    FallbackSecretStore(ISecretStore &primary, ISecretStore &fileStore, SettingsStore &settings,
                        ConsentPrompt prompt = {});

    void setConsentPrompt(ConsentPrompt prompt);

    void readPassword(const QString &accountId, ReadCallback callback) override;
    void writePassword(const QString &accountId, const QString &password,
                       WriteCallback callback = {}) override;
    void deletePassword(const QString &accountId, DeleteCallback callback = {}) override;
    bool backendUnavailable() const override;

private:
    using Continuation = std::function<void(bool useFile)>;

    bool usesFileStore() const;
    void resolveFallback(Continuation then);

    ISecretStore &primary_;
    ISecretStore &fileStore_;
    SettingsStore &settings_;
    ConsentPrompt prompt_;
    bool asking_ = false;
    QList<Continuation> waiting_;
};

} // namespace tincan
