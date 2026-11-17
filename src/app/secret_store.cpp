// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "app/secret_store.h"

#include <qt6keychain/keychain.h>

#include <QObject>
#include <utility>

namespace tincan {

KeychainSecretStore::KeychainSecretStore(QString serviceName)
    : serviceName_(std::move(serviceName)) {}

bool KeychainSecretStore::backendUnavailable() const { return backendUnavailable_; }

void KeychainSecretStore::noteError(int keychainError) {
    backendUnavailable_ = keychainError == QKeychain::NoBackendAvailable ||
                          keychainError == QKeychain::OtherError;
}

void KeychainSecretStore::readPassword(const QString &accountId, ReadCallback callback) {
    auto *job = new QKeychain::ReadPasswordJob(serviceName_);
    job->setAutoDelete(true);
    job->setKey(accountId);
    QObject::connect(job, &QKeychain::Job::finished, job,
                     [this, job, cb = std::move(callback)](QKeychain::Job *) {
                         noteError(job->error());
                         if (!cb) {
                             return;
                         }
                         if (job->error() == QKeychain::NoError) {
                             cb(job->textData(), QString{});
                         } else if (job->error() == QKeychain::EntryNotFound) {
                             cb(QString{}, QString{});
                         } else {
                             cb(QString{}, job->errorString());
                         }
                     });
    job->start();
}

void KeychainSecretStore::writePassword(const QString &accountId, const QString &password,
                                        WriteCallback callback) {
    if (password.isEmpty()) {
        deletePassword(accountId, std::move(callback));
        return;
    }

    auto *job = new QKeychain::WritePasswordJob(serviceName_);
    job->setAutoDelete(true);
    job->setKey(accountId);
    job->setTextData(password);
    QObject::connect(job, &QKeychain::Job::finished, job,
                     [this, job, cb = std::move(callback)](QKeychain::Job *) {
                         noteError(job->error());
                         if (!cb) {
                             return;
                         }
                         const bool ok = (job->error() == QKeychain::NoError);
                         cb(ok, ok ? QString{} : job->errorString());
                     });
    job->start();
}

void KeychainSecretStore::deletePassword(const QString &accountId, DeleteCallback callback) {
    auto *job = new QKeychain::DeletePasswordJob(serviceName_);
    job->setAutoDelete(true);
    job->setKey(accountId);
    QObject::connect(job, &QKeychain::Job::finished, job,
                     [this, job, cb = std::move(callback)](QKeychain::Job *) {
                         noteError(job->error());
                         if (!cb) {
                             return;
                         }
                         const bool ok = (job->error() == QKeychain::NoError ||
                                          job->error() == QKeychain::EntryNotFound);
                         cb(ok, ok ? QString{} : job->errorString());
                     });
    job->start();
}

void MemorySecretStore::readPassword(const QString &accountId, ReadCallback callback) {
    if (callback) {
        callback(secrets_.value(accountId), QString{});
    }
}

void MemorySecretStore::writePassword(const QString &accountId, const QString &password,
                                      WriteCallback callback) {
    if (password.isEmpty()) {
        secrets_.remove(accountId);
    } else {
        secrets_.insert(accountId, password);
    }
    if (callback) {
        callback(true, QString{});
    }
}

void MemorySecretStore::deletePassword(const QString &accountId, DeleteCallback callback) {
    secrets_.remove(accountId);
    if (callback) {
        callback(true, QString{});
    }
}

QString MemorySecretStore::peekPassword(const QString &accountId) const {
    return secrets_.value(accountId);
}

bool MemorySecretStore::contains(const QString &accountId) const {
    return secrets_.contains(accountId);
}

} // namespace tincan
