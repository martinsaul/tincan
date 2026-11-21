// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Tincan contributors

#include "core/account_config.h"
#include "core/dtmf_method.h"
#include "core/engine_config.h"
#include "pjsua_peer.h"
#include "recording_events.h"
#include "sip/pjsip_engine.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include <pjsua2/endpoint.hpp>
#include <pjsua2/media.hpp>

#include <algorithm>
#include <cmath>
#include <memory>
#include <numbers>
#include <vector>

namespace {

struct MonoPcm {
    int sampleRate = 0;
    std::vector<qint16> samples;
};

quint32 readLe(const QByteArray &data, qsizetype pos, int bytes) {
    quint32 value = 0;
    for (int i = bytes - 1; i >= 0; --i) {
        value = (value << 8) | static_cast<quint8>(data.at(pos + i));
    }
    return value;
}

// Minimal RIFF reader for the 16-bit mono WAV pjsua records.
MonoPcm readMonoWav(const QString &path) {
    QFile file(path);
    MonoPcm pcm;
    if (!file.open(QIODevice::ReadOnly)) {
        return pcm;
    }
    const QByteArray data = file.readAll();
    qsizetype pos = 12;
    while (data.startsWith("RIFF") && pos + 8 <= data.size()) {
        const QByteArray id = data.mid(pos, 4);
        const qsizetype size = readLe(data, pos + 4, 4);
        const qsizetype body = pos + 8;
        if (id == "fmt " && body + 16 <= data.size()) {
            pcm.sampleRate = static_cast<int>(readLe(data, body + 4, 4));
        } else if (id == "data") {
            const bool sized = size > 0 && body + size <= data.size();
            const qsizetype end = sized ? body + size : data.size();
            for (qsizetype p = body; p + 1 < end; p += 2) {
                pcm.samples.push_back(static_cast<qint16>(readLe(data, p, 2)));
            }
            break;
        }
        pos = body + size + (size % 2);
    }
    return pcm;
}

// Amplitude of `hz` in the 40 ms Hann-windowed block at `start` (Goertzel).
double blockAmplitude(const MonoPcm &pcm, std::size_t start, double hz) {
    const std::size_t n = static_cast<std::size_t>(pcm.sampleRate / 25);
    const double coeff = 2.0 * std::cos(2.0 * std::numbers::pi * hz / pcm.sampleRate);
    double s1 = 0.0;
    double s2 = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        const double w = 0.5 - 0.5 * std::cos(2.0 * std::numbers::pi * i / (n - 1));
        const double s0 = w * pcm.samples[start + i] + coeff * s1 - s2;
        s2 = s1;
        s1 = s0;
    }
    const double power = s1 * s1 + s2 * s2 - coeff * s1 * s2;
    return 2.0 * std::sqrt(std::max(power, 0.0)) / (n / 2.0);
}

// Largest amplitude of `hz` over the blocks that fit in [0, endSample).
double peakAmplitude(const MonoPcm &pcm, double hz, std::size_t endSample) {
    const std::size_t n = static_cast<std::size_t>(pcm.sampleRate / 25);
    endSample = std::min(endSample, pcm.samples.size());
    double peak = 0.0;
    for (std::size_t start = 0; n > 0 && start + n <= endSample; start += n / 2) {
        peak = std::max(peak, blockAmplitude(pcm, start, hz));
    }
    return peak;
}

// First sample of the first block where `hz` exceeds `threshold`, or the end.
std::size_t onsetOf(const MonoPcm &pcm, double hz, double threshold) {
    const std::size_t n = static_cast<std::size_t>(pcm.sampleRate / 25);
    for (std::size_t start = 0; n > 0 && start + n <= pcm.samples.size(); start += n / 2) {
        if (blockAmplitude(pcm, start, hz) > threshold) {
            return start;
        }
    }
    return pcm.samples.size();
}

std::vector<int> listenersOf(int portId) {
    if (portId < 0) {
        return {};
    }
    try {
        return pj::AudioMedia::getPortInfoFromId(portId).listeners;
    } catch (const pj::Error &) {
        return {};
    }
}

unsigned activePorts() { return pj::Endpoint::instance().mediaActivePorts(); }

} // namespace

// Local keypad feedback tones: one reusable generator wired to the speaker
// only, released when idle, and never heard by the far end.
class TestE2eKeypadTones : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() {
        events_ = std::make_unique<tincan::test::RecordingEvents>();
        engine_ = std::make_unique<tincan::sip::PjsipEngine>(*events_);
        tincan::EngineConfig cfg;
        cfg.udpPort = 0;
        cfg.enableTcp = false;
        cfg.nullAudio = true;
        QString error;
        QVERIFY2(engine_->start(cfg, &error), qPrintable(error));
        tincan::AccountConfig localAcc;
        localAcc.id = QStringLiteral("local");
        localAcc.user = QStringLiteral("tincan");
        localAcc.domain = QStringLiteral("127.0.0.1");
        localAcc.registerAccount = false;
        QVERIFY2(engine_->addAccount(localAcc, &error), qPrintable(error));
    }

    void cleanupTestCase() {
        engine_->shutdown();
        engine_.reset();
        events_.reset();
    }

    void notStartedEngineIsSilentNoOp() {
        tincan::test::RecordingEvents events;
        tincan::sip::PjsipEngine idle(events);
        idle.playKeypadTone(QLatin1Char('5'));
        QCOMPARE(idle.keypadTonePortId(), -1);
    }

    void rapidPressesReuseOneGeneratorAndReleaseIt() {
        QTRY_COMPARE_WITH_TIMEOUT(engine_->keypadTonePortId(), -1, 5000);
        const unsigned baseline = activePorts();

        engine_->playKeypadTone(QLatin1Char('+'));
        QCOMPARE(engine_->keypadTonePortId(), -1);

        const QString keys = QStringLiteral("0123456789*#ABCD");
        engine_->playKeypadTone(keys.front());
        const int portId = engine_->keypadTonePortId();
        QVERIFY(portId >= 0);
        for (int round = 0; round < 5; ++round) {
            for (const QChar key : keys) {
                engine_->playKeypadTone(key);
                QCoreApplication::processEvents();
                QCOMPARE(engine_->keypadTonePortId(), portId);
                QVERIFY(activePorts() <= baseline + 1);
            }
            QTest::qWait(30);
        }
        QTRY_COMPARE_WITH_TIMEOUT(listenersOf(portId), std::vector<int>{0}, 2000);

        // Released after the idle period; the bridge removes ports asynchronously.
        QTRY_COMPARE_WITH_TIMEOUT(engine_->keypadTonePortId(), -1, 5000);
        QTRY_COMPARE_WITH_TIMEOUT(activePorts(), baseline, 5000);
    }

    // During a call the peer records everything it receives. The local tone
    // for '1' (697 + 1209 Hz) must be absent while RFC 4733 events still
    // arrive; an in-band '5' (770 + 1336 Hz) proves the recording hears audio.
    void peerReceivesDtmfEventsButNotTheLocalTone() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString wavPath = dir.filePath(QStringLiteral("peer.wav"));
        tincan::test::PjsuaPeer peer;
        peer.setExtraArgs({QStringLiteral("--rec-file=%1").arg(wavPath),
                           QStringLiteral("--auto-rec")});
        QVERIFY2(peer.start(200, 30), qPrintable(peer.output()));
        const int callId = connectCall(peer);
        QVERIFY(callId >= 0);

        // While an RFC 4733 event is being sent the stream sends no audio, which
        // would hide a leak; SIP INFO leaves RTP audio flowing. Alternate both.
        for (int i = 0; i < 6; ++i) {
            const auto method =
                i % 2 == 0 ? tincan::DtmfMethod::Rfc4733 : tincan::DtmfMethod::SipInfo;
            QVERIFY(engine_->sendDtmf(callId, QStringLiteral("1"), method));
            engine_->playKeypadTone(QLatin1Char('1'));
            QTRY_COMPARE_WITH_TIMEOUT(listenersOf(engine_->keypadTonePortId()),
                                      std::vector<int>{0}, 2000);
            QTest::qWait(150);
        }
        QTRY_VERIFY_WITH_TIMEOUT(
            peer.output().count(QStringLiteral(": 1, using RFC2833 method")) >= 3 &&
                peer.output().count(QStringLiteral("using SIP INFO method")) >= 3,
            10000);

        QTest::qWait(300);
        QVERIFY(engine_->sendDtmf(callId, QStringLiteral("5"), tincan::DtmfMethod::InBand));
        QTest::qWait(800);
        QVERIFY(engine_->hangup(callId));
        QTRY_VERIFY_WITH_TIMEOUT(engine_->calls().isEmpty(), 5000);
        peer.stop();

        assertOnlyControlToneRecorded(readMonoWav(wavPath));
    }

private:
    int connectCall(tincan::test::PjsuaPeer &peer) {
        QString error;
        const int callId = engine_->makeCall(QStringLiteral("local"), peer.sipUri(), &error);
        if (callId < 0) {
            qWarning() << error;
            return -1;
        }
        const bool up = QTest::qWaitFor(
            [&]() {
                const QList<tincan::CallInfo> calls = engine_->calls();
                return std::any_of(calls.begin(), calls.end(), [&](const tincan::CallInfo &c) {
                    return c.callId == callId && c.micConnected;
                });
            },
            10000);
        QTest::qWait(300);
        return up ? callId : -1;
    }

    static void assertOnlyControlToneRecorded(const MonoPcm &pcm) {
        QVERIFY2(pcm.sampleRate > 0 &&
                     pcm.samples.size() > static_cast<std::size_t>(pcm.sampleRate),
                 "peer recording missing or too short");
        const std::size_t all = pcm.samples.size();
        const double control770 = peakAmplitude(pcm, 770, all);
        const double control1336 = peakAmplitude(pcm, 1336, all);
        QVERIFY2(control770 > 300 && control1336 > 300, "in-band control tone not recorded");

        // The control burst's on/off edges splatter into nearby bins, so the
        // local tone is looked for only before it. That span holds every
        // local burst (six of 120 ms, 150 ms apart, after call setup).
        const std::size_t controlStart = onsetOf(pcm, 770, 300);
        QVERIFY2(controlStart >= static_cast<std::size_t>(pcm.sampleRate),
                 "control tone started too early to cover the local bursts");
        const double local697 = peakAmplitude(pcm, 697, controlStart);
        const double local1209 = peakAmplitude(pcm, 1209, controlStart);
        qInfo("peer amplitudes: 697=%.1f 1209=%.1f (before %.2f s) 770=%.1f 1336=%.1f", local697,
              local1209, double(controlStart) / pcm.sampleRate, control770, control1336);
        QVERIFY2(local697 < 60 && local1209 < 60, "local keypad tone leaked to the far end");
    }

    std::unique_ptr<tincan::test::RecordingEvents> events_;
    std::unique_ptr<tincan::sip::PjsipEngine> engine_;
};

QTEST_GUILESS_MAIN(TestE2eKeypadTones)
#include "tst_e2e_keypad_tones.moc"
