// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#pragma once

#include <QHash>
#include <QString>

#include <functional>

namespace tincan {

class ISecretStore {
public:
    using ReadCallback = std::function<void(const QString &password, const QString &error)>;
    using WriteCallback = std::function<void(bool ok, const QString &error)>;
    using DeleteCallback = std::function<void(bool ok, const QString &error)>;

    virtual ~ISecretStore() = default;

    virtual void readPassword(const QString &accountId, ReadCallback callback) = 0;
    virtual void writePassword(const QString &accountId, const QString &password,
                               WriteCallback callback = {}) = 0;
    virtual void deletePassword(const QString &accountId, DeleteCallback callback = {}) = 0;
};

class KeychainSecretStore : public ISecretStore {
public:
    explicit KeychainSecretStore(QString serviceName = QStringLiteral("tincan"));

    void readPassword(const QString &accountId, ReadCallback callback) override;
    void writePassword(const QString &accountId, const QString &password,
                       WriteCallback callback = {}) override;
    void deletePassword(const QString &accountId, DeleteCallback callback = {}) override;

private:
    QString serviceName_;
};

class MemorySecretStore : public ISecretStore {
public:
    void readPassword(const QString &accountId, ReadCallback callback) override;
    void writePassword(const QString &accountId, const QString &password,
                       WriteCallback callback = {}) override;
    void deletePassword(const QString &accountId, DeleteCallback callback = {}) override;

    QString peekPassword(const QString &accountId) const;
    bool contains(const QString &accountId) const;

private:
    QHash<QString, QString> secrets_;
};

} // namespace tincan
