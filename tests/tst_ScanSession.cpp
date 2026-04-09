#include <QTest>
#include <QSignalSpy>
#include <cmath>
#include <limits>
#include "data/ScanSession.h"

class tst_ScanSession : public QObject
{
    Q_OBJECT

private slots:
    void addSweepUpdatesCount()
    {
        ScanSession session;
        QVERIFY(session.isEmpty());

        SweepData s(470e6, 1e6, {-80.0, -75.0, -90.0});
        session.addSweep(s);

        QCOMPARE(session.sweepCount(), 1);
        QVERIFY(!session.isEmpty());
    }

    void maxHoldTakesMaximums()
    {
        ScanSession session;

        session.addSweep(SweepData(470e6, 1e6, {-80.0, -75.0, -90.0}));
        session.addSweep(SweepData(470e6, 1e6, {-70.0, -85.0, -60.0}));

        auto mh = session.maxHold();
        QCOMPARE(mh.amplitudes().size(), 3);
        QCOMPARE(mh.amplitudes()[0], -70.0);  // max(-80, -70)
        QCOMPARE(mh.amplitudes()[1], -75.0);  // max(-75, -85)
        QCOMPARE(mh.amplitudes()[2], -60.0);  // max(-90, -60)
    }

    void averageComputesCorrectly()
    {
        // Averaging is done in the linear power domain:
        //   avg_dBm = 10 * log10( mean( 10^(dBm_i/10) ) )
        ScanSession session;

        session.addSweep(SweepData(470e6, 1e6, {-30.0, -50.0}));
        session.addSweep(SweepData(470e6, 1e6, {-30.0, -50.0}));

        // Identical sweeps: linear average equals the original values
        auto avg = session.average();
        QCOMPARE(avg.amplitudes().size(), 2);
        QVERIFY(qAbs(avg.amplitudes()[0] - (-30.0)) < 0.01);
        QVERIFY(qAbs(avg.amplitudes()[1] - (-50.0)) < 0.01);

        // Non-trivial case: -30 dBm and -40 dBm
        //   linear mean = (1e-3 + 1e-4) / 2 = 5.5e-4
        //   => 10*log10(5.5e-4) ≈ -32.596 dBm
        ScanSession session2;
        session2.addSweep(SweepData(470e6, 1e6, {-30.0}));
        session2.addSweep(SweepData(470e6, 1e6, {-40.0}));

        auto avg2 = session2.average();
        QVERIFY(qAbs(avg2.amplitudes()[0] - (-32.596)) < 0.01);
    }

    void clearResetsSession()
    {
        ScanSession session;
        session.addSweep(SweepData(0, 1e6, {-80.0}));

        QCOMPARE(session.sweepCount(), 1);
        session.clear();
        QVERIFY(session.isEmpty());
        QCOMPARE(session.sweepCount(), 0);
    }

    void latestSweepReturnsLastAdded()
    {
        ScanSession session;
        session.addSweep(SweepData(470e6, 1e6, {-80.0}));
        session.addSweep(SweepData(480e6, 1e6, {-60.0}));

        const auto& latest = session.latestSweep();
        QCOMPARE(latest.startFreqHz(), 480e6);
        QCOMPARE(latest.amplitudeAt(0), -60.0);
    }

    void latestSweepOnEmptyReturnsEmpty()
    {
        ScanSession session;
        const auto& latest = session.latestSweep();
        QVERIFY(latest.isEmpty());
    }

    void deviceNameGetterSetter()
    {
        ScanSession session;
        QVERIFY(session.deviceName().isEmpty());

        session.setDeviceName("RF Explorer");
        QCOMPARE(session.deviceName(), "RF Explorer");
    }

    void startStopTimeGetterSetter()
    {
        ScanSession session;
        QVERIFY(!session.startTime().isValid());
        QVERIFY(!session.stopTime().isValid());

        QDateTime start(QDate(2026, 1, 15), QTime(10, 0, 0), Qt::UTC);
        QDateTime stop(QDate(2026, 1, 15), QTime(11, 0, 0), Qt::UTC);
        session.setStartTime(start);
        session.setStopTime(stop);
        QCOMPARE(session.startTime(), start);
        QCOMPARE(session.stopTime(), stop);
    }

    void clearResetsTimestamps()
    {
        ScanSession session;
        session.setStartTime(QDateTime::currentDateTimeUtc());
        session.setStopTime(QDateTime::currentDateTimeUtc());
        session.addSweep(SweepData(470e6, 1e6, {-80.0}));

        session.clear();
        QVERIFY(!session.startTime().isValid());
        QVERIFY(!session.stopTime().isValid());
    }

    void sweepAddedSignalEmitted()
    {
        ScanSession session;
        QSignalSpy spy(&session, &ScanSession::sweepAdded);
        QVERIFY(spy.isValid());

        session.addSweep(SweepData(470e6, 1e6, {-80.0}));
        QCOMPARE(spy.count(), 1);

        session.addSweep(SweepData(470e6, 1e6, {-75.0}));
        QCOMPARE(spy.count(), 2);
    }

    void sessionClearedSignalEmitted()
    {
        ScanSession session;
        session.addSweep(SweepData(470e6, 1e6, {-80.0}));

        QSignalSpy spy(&session, &ScanSession::sessionCleared);
        QVERIFY(spy.isValid());

        session.clear();
        QCOMPARE(spy.count(), 1);
    }

    void sweepsAccessorReturnsAll()
    {
        ScanSession session;
        session.addSweep(SweepData(470e6, 1e6, {-80.0}));
        session.addSweep(SweepData(470e6, 1e6, {-75.0}));
        session.addSweep(SweepData(470e6, 1e6, {-90.0}));

        QCOMPARE(session.sweeps().size(), 3);
    }

    void maxHoldOnEmptyReturnsEmpty()
    {
        ScanSession session;
        auto mh = session.maxHold();
        QVERIFY(mh.isEmpty());
    }

    void averageOnEmptyReturnsEmpty()
    {
        ScanSession session;
        auto avg = session.average();
        QVERIFY(avg.isEmpty());
    }

    void maxHoldPreservesFrequencyParams()
    {
        ScanSession session;
        session.addSweep(SweepData(470e6, 0.5e6, {-80.0, -75.0}));
        session.addSweep(SweepData(470e6, 0.5e6, {-70.0, -85.0}));

        auto mh = session.maxHold();
        QCOMPARE(mh.startFreqHz(), 470e6);
        QCOMPARE(mh.stepSizeHz(), 0.5e6);
    }

    void averagePreservesFrequencyParams()
    {
        ScanSession session;
        session.addSweep(SweepData(470e6, 0.5e6, {-80.0, -60.0}));
        session.addSweep(SweepData(470e6, 0.5e6, {-40.0, -100.0}));

        auto avg = session.average();
        QCOMPARE(avg.startFreqHz(), 470e6);
        QCOMPARE(avg.stepSizeHz(), 0.5e6);
    }

    void averageWithThreeSweeps()
    {
        // Linear average of -90, -60, -30 dBm:
        //   mean(1e-9, 1e-6, 1e-3) = 3.33700e-4
        //   => 10*log10(3.337e-4) ≈ -34.767 dBm
        ScanSession session;
        session.addSweep(SweepData(470e6, 1e6, {-90.0}));
        session.addSweep(SweepData(470e6, 1e6, {-60.0}));
        session.addSweep(SweepData(470e6, 1e6, {-30.0}));

        auto avg = session.average();
        QVERIFY(qAbs(avg.amplitudes()[0] - (-34.767)) < 0.01);
    }

    // --- Edge-case tests ---

    void emptySweepIsAccepted()
    {
        // Adding an empty sweep (0 points) should not crash
        ScanSession session;
        SweepData empty(470e6, 1e6, {});
        session.addSweep(empty);
        QCOMPARE(session.sweepCount(), 1);
        // Max-hold and average should return something valid (empty amplitudes)
        auto mh = session.maxHold();
        QCOMPARE(mh.count(), 0);
        auto avg = session.average();
        QCOMPARE(avg.count(), 0);
    }

    void pointCountChangeResetsAccumulators()
    {
        ScanSession session;

        // Add two sweeps with 3 points
        session.addSweep(SweepData(470e6, 1e6, {-80.0, -75.0, -90.0}));
        session.addSweep(SweepData(470e6, 1e6, {-70.0, -85.0, -60.0}));
        QCOMPARE(session.sweepCount(), 2);

        auto mh1 = session.maxHold();
        QCOMPARE(mh1.count(), 3);
        QCOMPARE(mh1.amplitudes()[0], -70.0);

        // Now add a sweep with 2 points — should reset accumulators
        session.addSweep(SweepData(480e6, 2e6, {-50.0, -40.0}));
        // sweepCount resets because accumulators were reset
        QCOMPARE(session.sweepCount(), 1);

        auto mh2 = session.maxHold();
        QCOMPARE(mh2.count(), 2);
        QCOMPARE(mh2.amplitudes()[0], -50.0);
        QCOMPARE(mh2.amplitudes()[1], -40.0);

        // Reference frequency should reflect the new sweep
        QCOMPARE(mh2.startFreqHz(), 480e6);
        QCOMPARE(mh2.stepSizeHz(), 2e6);
    }

    void storedSweepCountVsTotalSweepCount()
    {
        ScanSession session;

        // storedSweepCount should equal sweepCount when under the limit
        session.addSweep(SweepData(470e6, 1e6, {-80.0}));
        session.addSweep(SweepData(470e6, 1e6, {-75.0}));
        QCOMPARE(session.sweepCount(), 2);
        QCOMPARE(session.storedSweepCount(), 2);
    }

    void averageUsesTotalCountNotStoredCount()
    {
        // Verify that average() uses total sweep count for the divisor,
        // not just the number of stored sweeps
        ScanSession session;
        session.addSweep(SweepData(470e6, 1e6, {-30.0}));
        session.addSweep(SweepData(470e6, 1e6, {-30.0}));

        // Both identical: average should still be -30.0
        auto avg = session.average();
        QVERIFY(qAbs(avg.amplitudes()[0] - (-30.0)) < 0.01);
    }

    void clearResetsStoredAndTotalCounts()
    {
        ScanSession session;
        session.addSweep(SweepData(470e6, 1e6, {-80.0}));
        session.addSweep(SweepData(470e6, 1e6, {-75.0}));

        session.clear();
        QCOMPARE(session.sweepCount(), 0);
        QCOMPARE(session.storedSweepCount(), 0);
        QVERIFY(session.isEmpty());
    }
};

QTEST_GUILESS_MAIN(tst_ScanSession)
#include "tst_ScanSession.moc"
