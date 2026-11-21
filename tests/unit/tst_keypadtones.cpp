// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "app/audio_service.h"
#include "app/settings_store.h"
#include "core/dtmf_tone.h"
#include "fake_sip_engine.h"

#include <QTemporaryDir>
#include <QTest>

using tincan::DtmfTonePair;

class TestKeypadTones : public QObject {
    Q_OBJECT

private slots:
    void mapsEveryKeyToItsRowAndColumn_data() {
        QTest::addColumn<QChar>("key");
        QTest::addColumn<int>("lowHz");
        QTest::addColumn<int>("highHz");
        const char *rows[] = {"123A", "456B", "789C", "*0#D"};
        const int lows[] = {697, 770, 852, 941};
        const int highs[] = {1209, 1336, 1477, 1633};
        for (int r = 0; r < 4; ++r) {
            for (int c = 0; c < 4; ++c) {
                const QChar key = QLatin1Char(rows[r][c]);
                QTest::addRow("%c", rows[r][c]) << key << lows[r] << highs[c];
            }
        }
    }

    void mapsEveryKeyToItsRowAndColumn() {
        QFETCH(QChar, key);
        QFETCH(int, lowHz);
        QFETCH(int, highHz);
        const std::optional<DtmfTonePair> pair = tincan::dtmfTonePair(key);
        QVERIFY(pair.has_value());
        QCOMPARE(pair->lowHz, lowHz);
        QCOMPARE(pair->highHz, highHz);
    }

    void lowercaseLettersMatchUppercase() {
        for (const char ch : {'a', 'b', 'c', 'd'}) {
            QCOMPARE(tincan::dtmfTonePair(QLatin1Char(ch)),
                     tincan::dtmfTonePair(QChar(QLatin1Char(ch)).toUpper()));
        }
    }

    void otherCharactersHaveNoTone() {
        for (const QChar ch : {QChar(u'+'), QChar(u'E'), QChar(u'x'), QChar(u' '), QChar(u'@'),
                               QChar(u'\0'), QChar(0x0661) /* Arabic-Indic one */}) {
            QVERIFY2(!tincan::dtmfTonePair(ch).has_value(), qPrintable(QString(ch)));
        }
    }

    void audioServicePlaysOnlyWhenEnabledAndOnlyForKeys() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        tincan::SettingsStore settings(dir.filePath(QStringLiteral("tincan.ini")));
        tincan::test::FakeSipEngine engine;
        tincan::AudioService audio(engine, settings);

        for (const char *key : {"1", "#", "*", "+", "12", "", "x"}) {
            audio.playKeypadTone(QString::fromLatin1(key));
        }
        QCOMPARE(engine.keypadTones, QStringLiteral("1#*"));

        tincan::CallSettings cs = settings.callSettings();
        cs.keypadTones = false;
        settings.setCallSettings(cs);
        audio.playKeypadTone(QStringLiteral("5"));
        QCOMPARE(engine.keypadTones, QStringLiteral("1#*"));
    }
};

QTEST_GUILESS_MAIN(TestKeypadTones)
#include "tst_keypadtones.moc"
