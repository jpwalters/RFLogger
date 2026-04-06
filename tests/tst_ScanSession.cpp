#include <QTest>
#include <QSignalSpy>
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
        ScanSession session;

        session.addSweep(SweepData(470e6, 1e6, {-80.0, -60.0}));
        session.addSweep(SweepData(470e6, 1e6, {-40.0, -100.0}));

        auto avg = session.average();
        QCOMPARE(avg.amplitudes().size(), 2);
        QCOMPARE(avg.amplitudes()[0], -60.0);  // (-80 + -40) / 2
        QCOMPARE(avg.amplitudes()[1], -80.0);  // (-60 + -100) / 2
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
        ScanSession session;
        session.addSweep(SweepData(470e6, 1e6, {-90.0}));
        session.addSweep(SweepData(470e6, 1e6, {-60.0}));
        session.addSweep(SweepData(470e6, 1e6, {-30.0}));

        auto avg = session.average();
        QCOMPARE(avg.amplitudes()[0], -60.0);  // (-90 + -60 + -30) / 3
    }
};

QTEST_GUILESS_MAIN(tst_ScanSession)
#include "tst_ScanSession.moc"
