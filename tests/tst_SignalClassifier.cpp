#include <QTest>
#include "data/SignalClassifier.h"
#include "data/TvChannelMap.h"
#include "data/RfBandPlan.h"

class tst_SignalClassifier : public QObject
{
    Q_OBJECT

private:
    SignalClassifier makeClassifier()
    {
        SignalClassifier c;
        c.setTvChannelMap(TvChannelMap::usAtsc());
        c.setBandPlan(RfBandPlan::us());
        return c;
    }

private slots:
    void classifyWirelessMicInUhf()
    {
        auto c = makeClassifier();
        DetectedSignal sig;
        sig.centerFreqHz = 518.0e6; // Inside UHF TV band / Part 74
        sig.peakAmplitudeDbm = -60.0;
        sig.bandwidthHz = 25000.0;  // 25 kHz — typical wireless mic

        auto result = c.classify(sig);
        QCOMPARE(result.type, "Wireless Mic");
    }

    void classifyDtvBroadcast()
    {
        auto c = makeClassifier();
        DetectedSignal sig;
        sig.centerFreqHz = 479.0e6; // CH 15 center
        sig.peakAmplitudeDbm = -50.0;
        sig.bandwidthHz = 5000000.0; // 5 MHz wide — DTV

        auto result = c.classify(sig);
        QCOMPARE(result.type, "DTV Ch 15");
    }

    void classifyLteSignal()
    {
        auto c = makeClassifier();
        DetectedSignal sig;
        sig.centerFreqHz = 631.0e6; // LTE Band 71
        sig.peakAmplitudeDbm = -70.0;
        sig.bandwidthHz = 10000000.0; // 10 MHz

        auto result = c.classify(sig);
        QCOMPARE(result.type, "LTE/Cellular");
    }

    void classifyNarrowband()
    {
        auto c = makeClassifier();
        DetectedSignal sig;
        sig.centerFreqHz = 400.0e6; // 400 MHz — not in any US band plan range
        sig.peakAmplitudeDbm = -80.0;
        sig.bandwidthHz = 10000.0; // 10 kHz

        auto result = c.classify(sig);
        QCOMPARE(result.type, "Narrowband");
    }

    void classifyUnknown()
    {
        auto c = makeClassifier();
        DetectedSignal sig;
        sig.centerFreqHz = 350.0e6; // Not in any tracked band
        sig.peakAmplitudeDbm = -65.0;
        sig.bandwidthHz = 500000.0; // 500 kHz

        auto result = c.classify(sig);
        QCOMPARE(result.type, "Unknown");
    }

    void classifyIsmBand()
    {
        auto c = makeClassifier();
        DetectedSignal sig;
        sig.centerFreqHz = 915.0e6; // ISM 900
        sig.peakAmplitudeDbm = -55.0;
        sig.bandwidthHz = 200000.0;

        auto result = c.classify(sig);
        QCOMPARE(result.type, "ISM 900");
    }

    void classifyPublicSafety()
    {
        auto c = makeClassifier();
        DetectedSignal sig;
        sig.centerFreqHz = 155.0e6; // Public safety VHF
        sig.peakAmplitudeDbm = -60.0;
        sig.bandwidthHz = 12500.0;

        auto result = c.classify(sig);
        // Public safety takes priority as band classification
        QCOMPARE(result.type, "Public Safety");
    }

    void descriptionIsNonEmpty()
    {
        auto c = makeClassifier();
        DetectedSignal sig;
        sig.centerFreqHz = 518.0e6;
        sig.peakAmplitudeDbm = -60.0;
        sig.bandwidthHz = 25000.0;

        auto result = c.classify(sig);
        QVERIFY(!result.description.isEmpty());
    }

    void classifyWithEmptyBandPlan()
    {
        // Should still work, just fall back to bandwidth-based classification
        SignalClassifier c;
        DetectedSignal sig;
        sig.centerFreqHz = 500.0e6;
        sig.peakAmplitudeDbm = -60.0;
        sig.bandwidthHz = 15000.0;

        auto result = c.classify(sig);
        QCOMPARE(result.type, "Narrowband");
    }

    void classifyEuWirelessMic()
    {
        SignalClassifier c;
        c.setTvChannelMap(TvChannelMap::euDvbt());
        c.setBandPlan(RfBandPlan::eu());

        DetectedSignal sig;
        sig.centerFreqHz = 610.0e6; // PMSE channel 38 area
        sig.peakAmplitudeDbm = -55.0;
        sig.bandwidthHz = 30000.0;

        auto result = c.classify(sig);
        QCOMPARE(result.type, "Wireless Mic");
    }

    // ── groupByChannel tests ─────────────────────────────────────────────

    void groupMergesDtvPeaks()
    {
        auto c = makeClassifier();

        // Simulate DTV Ch 18 (494–500 MHz): pilot tone + wide carrier + another peak
        QVector<DetectedSignal> sigs;
        sigs.append({494.31e6, -50.0, 40000.0, {}});     // pilot (~40 kHz)
        sigs.append({497.0e6,  -30.0, 4000000.0, {}});   // main DTV carrier
        sigs.append({498.5e6,  -45.0, 500000.0, {}});    // secondary peak

        auto grouped = c.groupByChannel(sigs);

        // Should merge into 1 DTV entry
        QCOMPARE(grouped.size(), 1);
        QCOMPARE(grouped[0].peakAmplitudeDbm, -30.0);           // strongest peak
        QVERIFY(grouped[0].bandwidthHz > 5000000.0);            // channel width ~6 MHz
        QVERIFY(grouped[0].label.contains("Ch 18"));
    }

    void groupKeepsNarrowMicsIndividual()
    {
        auto c = makeClassifier();

        // Two narrow signals in TV CH 20 (506–512 MHz) — no wide DTV peak
        QVector<DetectedSignal> sigs;
        sigs.append({507.0e6, -55.0, 25000.0, {}});    // wireless mic
        sigs.append({509.0e6, -60.0, 30000.0, {}});    // another mic / IEM

        auto grouped = c.groupByChannel(sigs);

        // No DTV activity → keep both individual
        QCOMPARE(grouped.size(), 2);
    }

    void groupMergesLtePeaks()
    {
        auto c = makeClassifier();

        // Two peaks in LTE Band 71 (617–652 MHz) — outside TV channel range
        // US ATSC channels go up to CH 36 (602–608 MHz), so 620+ is free
        QVector<DetectedSignal> sigs;
        sigs.append({625.0e6, -65.0, 5000000.0, {}});
        sigs.append({640.0e6, -70.0, 3000000.0, {}});

        auto grouped = c.groupByChannel(sigs);

        // Should merge into 1 LTE entry
        QCOMPARE(grouped.size(), 1);
        QCOMPARE(grouped[0].peakAmplitudeDbm, -65.0);
        QVERIFY(!grouped[0].label.isEmpty());
    }

    void groupPreservesUngrouped()
    {
        auto c = makeClassifier();

        // One wide DTV signal in Ch 18, one random signal outside any band
        QVector<DetectedSignal> sigs;
        sigs.append({497.0e6,  -30.0, 4000000.0, {}});  // DTV Ch 18 (wide → merged)
        sigs.append({400.0e6,  -70.0, 15000.0, {}});    // random narrowband (kept)

        auto grouped = c.groupByChannel(sigs);

        // DTV peak becomes a channel-level entry, random signal passes through
        QCOMPARE(grouped.size(), 2);
    }

    void groupEmptyInput()
    {
        auto c = makeClassifier();
        QVector<DetectedSignal> empty;
        auto grouped = c.groupByChannel(empty);
        QVERIFY(grouped.isEmpty());
    }
};

QTEST_GUILESS_MAIN(tst_SignalClassifier)
#include "tst_SignalClassifier.moc"
