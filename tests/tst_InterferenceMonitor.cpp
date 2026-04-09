#include <QTest>
#include <QSignalSpy>
#include "data/InterferenceMonitor.h"

class tst_InterferenceMonitor : public QObject
{
    Q_OBJECT

private slots:
    void alertTriggeredOnExceedance()
    {
        InterferenceMonitor monitor;
        monitor.setEnabled(true);
        monitor.setThresholdDb(6.0);
        monitor.setCooldownMs(0); // Disable cooldown for testing

        // Baseline: quiet environment
        SweepData baseline(470e6, 1e6, {-90.0, -90.0, -90.0, -90.0, -90.0});
        monitor.setBaseline(baseline);

        QSignalSpy spy(&monitor, &InterferenceMonitor::alertTriggered);
        QVERIFY(spy.isValid());

        // Sweep with interference at index 2: -90 + 10 dB = -80 dBm (exceeds threshold)
        SweepData live(470e6, 1e6, {-90.0, -90.0, -80.0, -90.0, -90.0});
        monitor.checkSweep(live);

        QCOMPARE(spy.count(), 1);

        auto alert = spy.at(0).at(0).value<InterferenceAlert>();
        QVERIFY(qAbs(alert.frequencyHz - 472e6) < 1.0); // Index 2: 470 + 2*1 = 472 MHz
        QCOMPARE(alert.amplitudeDbm, -80.0);
    }

    void noAlertBelowThreshold()
    {
        InterferenceMonitor monitor;
        monitor.setEnabled(true);
        monitor.setThresholdDb(6.0);
        monitor.setCooldownMs(0);

        SweepData baseline(470e6, 1e6, {-90.0, -90.0, -90.0});
        monitor.setBaseline(baseline);

        QSignalSpy spy(&monitor, &InterferenceMonitor::alertTriggered);

        // Only 3 dB above baseline — below 6 dB threshold
        SweepData live(470e6, 1e6, {-87.0, -87.0, -87.0});
        monitor.checkSweep(live);

        QCOMPARE(spy.count(), 0);
    }

    void disabledMonitorNoAlerts()
    {
        InterferenceMonitor monitor;
        monitor.setEnabled(false);
        monitor.setThresholdDb(6.0);
        monitor.setCooldownMs(0);

        SweepData baseline(470e6, 1e6, {-90.0, -90.0});
        monitor.setBaseline(baseline);

        QSignalSpy spy(&monitor, &InterferenceMonitor::alertTriggered);

        SweepData live(470e6, 1e6, {-50.0, -50.0}); // Way above baseline
        monitor.checkSweep(live);

        QCOMPARE(spy.count(), 0);
    }

    void noBaselineNoAlerts()
    {
        InterferenceMonitor monitor;
        monitor.setEnabled(true);
        monitor.setCooldownMs(0);

        QSignalSpy spy(&monitor, &InterferenceMonitor::alertTriggered);

        SweepData live(470e6, 1e6, {-50.0, -50.0});
        monitor.checkSweep(live);

        QCOMPARE(spy.count(), 0);
    }

    void watchFrequencyMonitoring()
    {
        InterferenceMonitor monitor;
        monitor.setEnabled(true);
        monitor.setThresholdDb(6.0);
        monitor.setCooldownMs(0);

        // Baseline: quiet
        SweepData baseline(470e6, 1e6, {-90.0, -90.0, -90.0, -90.0, -90.0});
        monitor.setBaseline(baseline);

        // Only watch around 472 MHz (index 2)
        monitor.setWatchFrequencies({472e6}, 0.5e6);

        QSignalSpy spy(&monitor, &InterferenceMonitor::alertTriggered);

        // Interference at index 0 (470 MHz) — NOT watched
        // Clean at index 2 (472 MHz) — watched but clean
        SweepData live1(470e6, 1e6, {-70.0, -90.0, -90.0, -90.0, -90.0});
        monitor.checkSweep(live1);
        QCOMPARE(spy.count(), 0); // No alert — interference is outside watch range

        // Now interference at watched frequency
        SweepData live2(470e6, 1e6, {-90.0, -90.0, -70.0, -90.0, -90.0});
        monitor.checkSweep(live2);
        QCOMPARE(spy.count(), 1);
    }

    void cooldownPreventsRapidAlerts()
    {
        InterferenceMonitor monitor;
        monitor.setEnabled(true);
        monitor.setThresholdDb(6.0);
        monitor.setCooldownMs(5000); // 5 second cooldown

        SweepData baseline(470e6, 1e6, {-90.0, -90.0});
        monitor.setBaseline(baseline);

        QSignalSpy spy(&monitor, &InterferenceMonitor::alertTriggered);

        SweepData live(470e6, 1e6, {-70.0, -70.0});
        monitor.checkSweep(live);
        QCOMPARE(spy.count(), 1);

        // Second check should be suppressed by cooldown
        monitor.checkSweep(live);
        QCOMPARE(spy.count(), 1);
    }

    void alertDescriptionContainsFrequency()
    {
        InterferenceMonitor monitor;
        monitor.setEnabled(true);
        monitor.setThresholdDb(6.0);
        monitor.setCooldownMs(0);

        SweepData baseline(470e6, 1e6, {-90.0});
        monitor.setBaseline(baseline);

        QSignalSpy spy(&monitor, &InterferenceMonitor::alertTriggered);

        SweepData live(470e6, 1e6, {-70.0});
        monitor.checkSweep(live);

        QCOMPARE(spy.count(), 1);
        auto alert = spy.at(0).at(0).value<InterferenceAlert>();
        QVERIFY(alert.description.contains("470"));
        QVERIFY(alert.description.contains("MHz"));
    }
};

QTEST_GUILESS_MAIN(tst_InterferenceMonitor)
#include "tst_InterferenceMonitor.moc"
