// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "app/fallback_secret_store.h"

#include <utility>

namespace tincan {

FallbackSecretStore::FallbackSecretStore(ISecretStore &primary, ISecretStore &fileStore,
                                         SettingsStore &settings, ConsentPrompt prompt)
    : primary_(primary), fileStore_(fileStore), settings_(settings), prompt_(std::move(prompt)) {}

void FallbackSecretStore::setConsentPrompt(ConsentPrompt prompt) { prompt_ = std::move(prompt); }

bool FallbackSecretStore::usesFileStore() const {
    return settings_.secretFallbackChoice() == SecretFallbackChoice::ConfigFile;
}

bool FallbackSecretStore::backendUnavailable() const {
    return !usesFileStore() && primary_.backendUnavailable();
}

void FallbackSecretStore::resolveFallback(Continuation then) {
    const SecretFallbackChoice choice = settings_.secretFallbackChoice();
    if (choice != SecretFallbackChoice::Undecided) {
        then(choice == SecretFallbackChoice::ConfigFile);
        return;
    }
    waiting_.append(std::move(then));
    if (asking_) {
        return; // a prompt is already open; answer every waiter together
    }
    asking_ = true;
    const bool allowFile = prompt_ ? prompt_() : false;
    settings_.setSecretFallbackChoice(allowFile ? SecretFallbackChoice::ConfigFile
                                                : SecretFallbackChoice::Declined);
    asking_ = false;
    const QList<Continuation> waiters = std::exchange(waiting_, {});
    for (const Continuation &waiter : waiters) {
        waiter(allowFile);
    }
}

void FallbackSecretStore::readPassword(const QString &accountId, ReadCallback callback) {
    if (usesFileStore()) {
        fileStore_.readPassword(accountId, std::move(callback));
        return;
    }
    primary_.readPassword(accountId, [this, accountId, cb = std::move(callback)](
                                         const QString &password, const QString &error) {
        if (error.isEmpty() || !primary_.backendUnavailable()) {
            if (cb) {
                cb(password, error);
            }
            return;
        }
        resolveFallback([this, accountId, cb, error](bool useFile) {
            if (useFile) {
                fileStore_.readPassword(accountId, cb);
            } else if (cb) {
                cb(QString{}, error);
            }
        });
    });
}

void FallbackSecretStore::writePassword(const QString &accountId, const QString &password,
                                        WriteCallback callback) {
    if (usesFileStore()) {
        fileStore_.writePassword(accountId, password, std::move(callback));
        return;
    }
    primary_.writePassword(accountId, password, [this, accountId, password,
                                                 cb = std::move(callback)](bool ok,
                                                                           const QString &error) {
        if (ok || !primary_.backendUnavailable()) {
            if (cb) {
                cb(ok, error);
            }
            return;
        }
        resolveFallback([this, accountId, password, cb, error](bool useFile) {
            if (useFile) {
                fileStore_.writePassword(accountId, password, cb);
            } else if (cb) {
                cb(false, error);
            }
        });
    });
}

void FallbackSecretStore::deletePassword(const QString &accountId, DeleteCallback callback) {
    if (usesFileStore()) {
        fileStore_.deletePassword(accountId, std::move(callback));
        return;
    }
    primary_.deletePassword(accountId, [this, accountId, cb = std::move(callback)](
                                           bool ok, const QString &error) {
        if (!ok && primary_.backendUnavailable()) {
            // Nothing can be stored in a missing keychain; a delete is moot.
            // Still clear any copy in the file store, without prompting.
            fileStore_.deletePassword(accountId, cb);
            return;
        }
        if (cb) {
            cb(ok, error);
        }
    });
}

} // namespace tincan
