// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "app/config_secret_store.h"
#include "app/fallback_secret_store.h"
#include "app/settings_store.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

namespace {

// A keychain stand-in that can pretend no backend exists.
class FakeKeychain : public tincan::MemorySecretStore {
public:
    bool missing = false;

    void readPassword(const QString &id, ReadCallback cb) override {
        if (missing) {
            cb(QString{}, QStringLiteral("no backend"));
            return;
        }
        MemorySecretStore::readPassword(id, std::move(cb));
    }
    void writePassword(const QString &id, const QString &pw, WriteCallback cb = {}) override {
        if (missing) {
            cb(false, QStringLiteral("no backend"));
            return;
        }
        MemorySecretStore::writePassword(id, pw, std::move(cb));
    }
    bool backendUnavailable() const override { return missing; }
};

QString readSync(tincan::ISecretStore &store, const QString &id, QString *error = nullptr) {
    QString result;
    store.readPassword(id, [&](const QString &pw, const QString &err) {
        result = pw;
        if (error != nullptr) {
            *error = err;
        }
    });
    return result;
}

bool writeSync(tincan::ISecretStore &store, const QString &id, const QString &pw) {
    bool result = false;
    store.writePassword(id, pw, [&](bool ok, const QString &) { result = ok; });
    return result;
}

constexpr QFileDevice::Permissions kOwnerRw = QFileDevice::ReadOwner | QFileDevice::WriteOwner;

} // namespace

class TestConfigSecretStore : public QObject {
    Q_OBJECT

private slots:
    void storesPasswordsInOwnerOnlyFile() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("cfg/secrets.ini"));
        tincan::ConfigSecretStore store(path);

        QCOMPARE(readSync(store, QStringLiteral("acc-1")), QString{});
        QVERIFY(writeSync(store, QStringLiteral("acc-1"), QStringLiteral("s3cr=t;[x]")));
        QVERIFY(writeSync(store, QStringLiteral("acc/2"), QStringLiteral("other")));
        QVERIFY((QFile::permissions(path) & kOwnerRw) == kOwnerRw);
        QVERIFY(!(QFile::permissions(path) &
                  (QFileDevice::ReadGroup | QFileDevice::ReadOther | QFileDevice::WriteGroup |
                   QFileDevice::WriteOther | QFileDevice::ExeOwner)));

        tincan::ConfigSecretStore reopened(path);
        QCOMPARE(readSync(reopened, QStringLiteral("acc-1")), QStringLiteral("s3cr=t;[x]"));
        QCOMPARE(readSync(reopened, QStringLiteral("acc/2")), QStringLiteral("other"));

        bool deleted = false;
        reopened.deletePassword(QStringLiteral("acc-1"), [&](bool ok, const QString &) {
            deleted = ok;
        });
        QVERIFY(deleted);
        QCOMPARE(readSync(reopened, QStringLiteral("acc-1")), QString{});
        QVERIFY(writeSync(reopened, QStringLiteral("acc/2"), QString{})); // empty == delete
        QCOMPARE(readSync(reopened, QStringLiteral("acc/2")), QString{});
    }

    void tightensExistingLoosePermissions() {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("secrets.ini"));
        QFile loose(path);
        QVERIFY(loose.open(QIODevice::WriteOnly));
        loose.close();
        QVERIFY(QFile::setPermissions(path, kOwnerRw | QFileDevice::ReadOther));
        tincan::ConfigSecretStore store(path);
        QVERIFY(writeSync(store, QStringLiteral("a"), QStringLiteral("b")));
        QVERIFY(!(QFile::permissions(path) & QFileDevice::ReadOther));
    }

    void fallbackAsksOnceAndUsesFileWhenAllowed() {
        QTemporaryDir dir;
        tincan::SettingsStore settings(dir.filePath(QStringLiteral("tincan.ini")));
        FakeKeychain keychain;
        keychain.missing = true;
        tincan::ConfigSecretStore file(dir.filePath(QStringLiteral("secrets.ini")));
        int prompts = 0;
        tincan::FallbackSecretStore store(keychain, file, settings, [&]() {
            ++prompts;
            return true;
        });

        QVERIFY(writeSync(store, QStringLiteral("acc"), QStringLiteral("pw")));
        QCOMPARE(readSync(store, QStringLiteral("acc")), QStringLiteral("pw"));
        QCOMPARE(prompts, 1);
        QCOMPARE(settings.secretFallbackChoice(), tincan::SecretFallbackChoice::ConfigFile);
        QCOMPARE(readSync(file, QStringLiteral("acc")), QStringLiteral("pw"));
        QFile mainConfig(dir.filePath(QStringLiteral("tincan.ini")));
        QVERIFY(mainConfig.open(QIODevice::ReadOnly));
        QVERIFY(!mainConfig.readAll().contains("pw"));

        // The remembered choice survives a restart: no second prompt.
        tincan::SettingsStore reloaded(dir.filePath(QStringLiteral("tincan.ini")));
        tincan::FallbackSecretStore again(keychain, file, reloaded, [&]() {
            ++prompts;
            return false;
        });
        QCOMPARE(readSync(again, QStringLiteral("acc")), QStringLiteral("pw"));
        QCOMPARE(prompts, 1);
    }

    void fallbackDeclinedIsRememberedAndNothingIsWritten() {
        QTemporaryDir dir;
        tincan::SettingsStore settings(dir.filePath(QStringLiteral("tincan.ini")));
        FakeKeychain keychain;
        keychain.missing = true;
        const QString filePath = dir.filePath(QStringLiteral("secrets.ini"));
        tincan::ConfigSecretStore file(filePath);
        int prompts = 0;
        tincan::FallbackSecretStore store(keychain, file, settings, [&]() {
            ++prompts;
            return false;
        });

        QVERIFY(!writeSync(store, QStringLiteral("acc"), QStringLiteral("pw")));
        QString error;
        QCOMPARE(readSync(store, QStringLiteral("acc"), &error), QString{});
        QVERIFY(!error.isEmpty());
        QCOMPARE(prompts, 1);
        QCOMPARE(settings.secretFallbackChoice(), tincan::SecretFallbackChoice::Declined);
        QVERIFY(!QFile::exists(filePath));
    }

    void workingKeychainNeverPrompts() {
        QTemporaryDir dir;
        tincan::SettingsStore settings(dir.filePath(QStringLiteral("tincan.ini")));
        FakeKeychain keychain;
        tincan::ConfigSecretStore file(dir.filePath(QStringLiteral("secrets.ini")));
        int prompts = 0;
        tincan::FallbackSecretStore store(keychain, file, settings, [&]() {
            ++prompts;
            return true;
        });
        QVERIFY(writeSync(store, QStringLiteral("acc"), QStringLiteral("pw")));
        QCOMPARE(readSync(store, QStringLiteral("acc")), QStringLiteral("pw"));
        QCOMPARE(keychain.peekPassword(QStringLiteral("acc")), QStringLiteral("pw"));
        QCOMPARE(prompts, 0);
        QCOMPARE(settings.secretFallbackChoice(), tincan::SecretFallbackChoice::Undecided);
    }
};

QTEST_GUILESS_MAIN(TestConfigSecretStore)
#include "tst_configsecretstore.moc"
