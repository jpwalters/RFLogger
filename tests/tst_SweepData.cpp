#include <QTest>
#include "data/SweepData.h"

class tst_SweepData : public QObject
{
    Q_OBJECT

private slots:
    void frequencyAtIndex()
    {
        SweepData s(470e6, 1e6, {-80.0, -75.0, -90.0, -60.0});

        QCOMPARE(s.frequencyAtIndex(0), 470e6);
        QCOMPARE(s.frequencyAtIndex(2), 472e6);
    }

    void stopFreqHz()
    {
        QVector<double> amps(10, -80.0);
        SweepData s(470e6, 0.5e6, amps);

        // stop = start + step * (count - 1)
        QCOMPARE(s.stopFreqHz(), 470e6 + 0.5e6 * 9);
    }

    void minMaxAmplitude()
    {
        SweepData s(0, 1e6, {-100.0, -50.0, -85.5, -30.0, -120.0});

        QCOMPARE(s.minAmplitude(), -120.0);
        QCOMPARE(s.maxAmplitude(), -30.0);
    }

    void emptyAmplitudes()
    {
        SweepData s;
        QVERIFY(s.isEmpty());
        QCOMPARE(s.stopFreqHz(), s.startFreqHz());
    }
};

QTEST_GUILESS_MAIN(tst_SweepData)
#include "tst_SweepData.moc"
