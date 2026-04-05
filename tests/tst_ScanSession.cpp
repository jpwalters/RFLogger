#include <QTest>
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
};

QTEST_GUILESS_MAIN(tst_ScanSession)
#include "tst_ScanSession.moc"
