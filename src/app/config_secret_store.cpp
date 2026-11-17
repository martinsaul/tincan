// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "app/config_secret_store.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>

#include <utility>

namespace tincan {

namespace {

constexpr QFileDevice::Permissions kOwnerOnly = QFileDevice::ReadOwner | QFileDevice::WriteOwner;

struct Text {
    Q_DECLARE_TR_FUNCTIONS(tincan::ConfigSecretStore)
};

QString writeError(const QString &path) {
    return Text::tr("Cannot write the password file %1.").arg(path);
}

QString keyFor(const QString &accountId) {
    // Account ids are opaque; keep them from being read as INI group paths.
    return QStringLiteral("passwords/") + QString::fromLatin1(accountId.toUtf8().toHex());
}

} // namespace

ConfigSecretStore::ConfigSecretStore(QString filePath) : filePath_(std::move(filePath)) {}

QString ConfigSecretStore::filePath() const { return filePath_; }

bool ConfigSecretStore::ensurePrivateFile() const {
    if (!QDir().mkpath(QFileInfo(filePath_).absolutePath())) {
        return false;
    }
    QFile file(filePath_);
    if (!file.exists() && !file.open(QIODevice::WriteOnly, kOwnerOnly)) {
        return false;
    }
    return QFile::setPermissions(filePath_, kOwnerOnly);
}

bool ConfigSecretStore::store(const QString &accountId, const QString *password) {
    if (!ensurePrivateFile()) {
        return false;
    }
    {
        QSettings ini(filePath_, QSettings::IniFormat);
        if (password != nullptr) {
            ini.setValue(keyFor(accountId), *password);
        } else {
            ini.remove(keyFor(accountId));
        }
        ini.sync();
        if (ini.status() != QSettings::NoError) {
            return false;
        }
    }
    return QFile::setPermissions(filePath_, kOwnerOnly);
}

void ConfigSecretStore::readPassword(const QString &accountId, ReadCallback callback) {
    QString password;
    if (QFile::exists(filePath_)) {
        const QSettings ini(filePath_, QSettings::IniFormat);
        password = ini.value(keyFor(accountId)).toString();
    }
    if (callback) {
        callback(password, QString{});
    }
}

void ConfigSecretStore::writePassword(const QString &accountId, const QString &password,
                                      WriteCallback callback) {
    const bool ok = password.isEmpty() ? store(accountId, nullptr) : store(accountId, &password);
    if (callback) {
        callback(ok, ok ? QString{} : writeError(filePath_));
    }
}

void ConfigSecretStore::deletePassword(const QString &accountId, DeleteCallback callback) {
    const bool ok = store(accountId, nullptr);
    if (callback) {
        callback(ok, ok ? QString{} : writeError(filePath_));
    }
}

} // namespace tincan
