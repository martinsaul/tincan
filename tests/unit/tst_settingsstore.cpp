// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "app/settings_store.h"

#include <QFile>
#include <QFileDevice>
#include <QTemporaryDir>
#include <QTest>

namespace {

tincan::AccountConfig makeFullSampleAccount() {
    tincan::AccountConfig acc;
    acc.id = QStringLiteral("acc-1");
    acc.displayName = QStringLiteral("Alice Desk");
    acc.user = QStringLiteral("101");
    acc.domain = QStringLiteral("pbx.example.org");
    acc.authUser = QStringLiteral("auth101");
    acc.password = QStringLiteral("TopSecretPassword!99");
    acc.proxy = QStringLiteral("sip:proxy.example.org:5060");
    acc.transport = tincan::Transport::Tls;
    acc.srtp = tincan::SrtpMode::Mandatory;
    acc.registerAccount = true;
    acc.regExpirySec = 600;
    acc.stunServer = QStringLiteral("stun.example.org:3478");
    acc.ice = true;
    acc.publicAddress = QStringLiteral("203.0.113.10");
    acc.dialPrefix = QStringLiteral("9");
    acc.voicemailNumber = QStringLiteral("*97");
    acc.publishPresence = true;
    return acc;
}

} // namespace

class TestSettingsStore : public QObject {
    Q_OBJECT

private slots:
    void roundTripsAccountsAndNeverWritesPassword() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString iniPath = tempDir.filePath(QStringLiteral("tincan.ini"));
        const tincan::AccountConfig acc1 = makeFullSampleAccount();

        tincan::AccountConfig acc2;
        acc2.id = QStringLiteral("acc-2");
        acc2.user = QStringLiteral("bob");
        acc2.domain = QStringLiteral("192.0.2.5");
        acc2.password = QStringLiteral("AnotherSecret123");
        acc2.transport = tincan::Transport::Tcp;
        acc2.srtp = tincan::SrtpMode::Optional;
        acc2.registerAccount = false;

        {
            tincan::SettingsStore writer(iniPath);
            writer.setAccounts({acc1, acc2});
            writer.setDefaultAccountId(QStringLiteral("acc-1"));
        }

        QFile rawFile(iniPath);
        QVERIFY(rawFile.open(QIODevice::ReadOnly));
        const QByteArray rawBytes = rawFile.readAll();
        QVERIFY(!rawBytes.contains("TopSecretPassword!99"));
        QVERIFY(!rawBytes.contains("AnotherSecret123"));
        QVERIFY(!rawBytes.contains("password"));

        tincan::SettingsStore reader(iniPath);
        const QList<tincan::AccountConfig> loaded = reader.accounts();
        QCOMPARE(loaded.size(), 2);
        QCOMPARE(loaded[0].id, acc1.id);
        QCOMPARE(loaded[0].displayName, acc1.displayName);
        QCOMPARE(loaded[0].user, acc1.user);
        QCOMPARE(loaded[0].domain, acc1.domain);
        QCOMPARE(loaded[0].authUser, acc1.authUser);
        QVERIFY(loaded[0].password.isEmpty());
        QCOMPARE(loaded[0].proxy, acc1.proxy);
        QCOMPARE(loaded[0].transport, tincan::Transport::Tls);
        QCOMPARE(loaded[0].srtp, tincan::SrtpMode::Mandatory);
        QCOMPARE(loaded[0].regExpirySec, 600);
        QCOMPARE(loaded[0].stunServer, acc1.stunServer);
        QCOMPARE(loaded[0].ice, true);
        QCOMPARE(loaded[0].publicAddress, acc1.publicAddress);
        QCOMPARE(loaded[0].dialPrefix, acc1.dialPrefix);
        QCOMPARE(loaded[0].voicemailNumber, acc1.voicemailNumber);
        QCOMPARE(loaded[0].publishPresence, true);
        QCOMPARE(loaded[1].publishPresence, false);
        QVERIFY(loaded[1].password.isEmpty());
        QCOMPARE(loaded[1].transport, tincan::Transport::Tcp);
        QCOMPARE(loaded[1].srtp, tincan::SrtpMode::Optional);
        QCOMPARE(reader.defaultAccountId(), QStringLiteral("acc-1"));
    }

    void enforcesOwnerOnlyPermissionsOnConfigFile() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString iniPath = tempDir.filePath(QStringLiteral("tincan.ini"));

        tincan::SettingsStore store(iniPath);
        store.setDefaultAccountId(QStringLiteral("acc-1"));

        QFile rawFile(iniPath);
        QVERIFY(rawFile.exists());
        const QFileDevice::Permissions perms = rawFile.permissions();
        QVERIFY(perms.testFlag(QFileDevice::ReadOwner));
        QVERIFY(perms.testFlag(QFileDevice::WriteOwner));
        QVERIFY(!perms.testFlag(QFileDevice::ReadGroup));
        QVERIFY(!perms.testFlag(QFileDevice::WriteGroup));
        QVERIFY(!perms.testFlag(QFileDevice::ReadOther));
        QVERIFY(!perms.testFlag(QFileDevice::WriteOther));
    }

    void roundTripsGeneralAndAudioSettings() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString iniPath = tempDir.filePath(QStringLiteral("tincan.ini"));

        {
            tincan::SettingsStore writer(iniPath);
            tincan::GeneralSettings gen{true, true, true, QStringLiteral("/tmp/custom-ring.wav")};
            writer.setGeneralSettings(gen);

            tincan::AudioSettings aud;
            aud.captureName = QStringLiteral("USB Headset");
            aud.playbackName = QStringLiteral("Built-in Audio");
            aud.ringName = QStringLiteral("Desk Speaker");
            aud.echoCancel = false;
            aud.ecTailMs = 150;
            aud.codecOrder = {QStringLiteral("opus/48000/2"), QStringLiteral("PCMU/8000/1")};
            aud.disabledCodecs = {QStringLiteral("PCMA/8000/1")};
            writer.setAudioSettings(aud);
        }

        tincan::SettingsStore reader(iniPath);
        const tincan::GeneralSettings gen = reader.generalSettings();
        QCOMPARE(gen.startMinimized, true);
        QCOMPARE(gen.closeToTray, true);
        QCOMPARE(gen.autostart, true);
        QCOMPARE(gen.ringtonePath, QStringLiteral("/tmp/custom-ring.wav"));

        const tincan::AudioSettings aud = reader.audioSettings();
        QCOMPARE(aud.captureName, QStringLiteral("USB Headset"));
        QCOMPARE(aud.playbackName, QStringLiteral("Built-in Audio"));
        QCOMPARE(aud.ringName, QStringLiteral("Desk Speaker"));
        QCOMPARE(aud.echoCancel, false);
        QCOMPARE(aud.ecTailMs, 150);
        QCOMPARE(aud.codecOrder,
                 (QStringList{QStringLiteral("opus/48000/2"), QStringLiteral("PCMU/8000/1")}));
        QCOMPARE(aud.disabledCodecs, (QStringList{QStringLiteral("PCMA/8000/1")}));
    }

    void roundTripsCallSettingsAndDefaultsRecordingsDir() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString iniPath = tempDir.filePath(QStringLiteral("tincan.ini"));

        {
            tincan::SettingsStore defaultsReader(iniPath);
            const tincan::CallSettings def = defaultsReader.callSettings();
            QCOMPARE(def.dtmfMethod, tincan::DtmfMethod::Rfc4733);
            QCOMPARE(def.autoAnswerMode, tincan::AutoAnswerMode::Off);
            QVERIFY(!def.recordingsDir.isEmpty());
            QVERIFY(def.recordingsDir.endsWith(QStringLiteral("Tincan")) ||
                    def.recordingsDir.endsWith(QStringLiteral("Recordings")));
        }

        const QString customRecDir = tempDir.filePath(QStringLiteral("recs"));
        {
            tincan::SettingsStore writer(iniPath);
            tincan::CallSettings cs;
            cs.dtmfMethod = tincan::DtmfMethod::SipInfo;
            cs.autoAnswerMode = tincan::AutoAnswerMode::HeaderOnly;
            cs.autoAnswerDelaySec = 4;
            cs.dnd = true;
            cs.forwardAlways = QStringLiteral("sip:fwd1@example.org");
            cs.forwardOnBusy = QStringLiteral("sip:fwd2@example.org");
            cs.forwardOnNoAnswer = QStringLiteral("sip:fwd3@example.org");
            cs.forwardNoAnswerSec = 20;
            cs.recordingsDir = customRecDir;
            cs.autoRecord = true;
            writer.setCallSettings(cs);
        }

        tincan::SettingsStore reader(iniPath);
        const tincan::CallSettings loaded = reader.callSettings();
        QCOMPARE(loaded.dtmfMethod, tincan::DtmfMethod::SipInfo);
        QCOMPARE(loaded.autoAnswerMode, tincan::AutoAnswerMode::HeaderOnly);
        QCOMPARE(loaded.autoAnswerDelaySec, 4);
        QCOMPARE(loaded.dnd, true);
        QCOMPARE(loaded.forwardAlways, QStringLiteral("sip:fwd1@example.org"));
        QCOMPARE(loaded.forwardOnBusy, QStringLiteral("sip:fwd2@example.org"));
        QCOMPARE(loaded.forwardOnNoAnswer, QStringLiteral("sip:fwd3@example.org"));
        QCOMPARE(loaded.forwardNoAnswerSec, 20);
        QCOMPARE(loaded.recordingsDir, customRecDir);
        QCOMPARE(loaded.autoRecord, true);
    }

    void roundTripsWindowState() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString iniPath = tempDir.filePath(QStringLiteral("tincan.ini"));
        {
            tincan::SettingsStore fresh(iniPath);
            QVERIFY(fresh.windowState().geometry.isEmpty());
            QCOMPARE(fresh.windowState().lastTab, 0);
            fresh.setWindowState(tincan::WindowState{QByteArray("\x01\x02geometry"), 2});
        }
        tincan::SettingsStore reader(iniPath);
        QCOMPARE(reader.windowState().geometry, QByteArray("\x01\x02geometry"));
        QCOMPARE(reader.windowState().lastTab, 2);
    }
};

QTEST_GUILESS_MAIN(TestSettingsStore)
#include "tst_settingsstore.moc"
