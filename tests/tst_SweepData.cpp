#include <QTest>
#include <cmath>
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

    void countMatchesInput()
    {
        SweepData s(470e6, 1e6, {-80.0, -75.0, -90.0});
        QCOMPARE(s.count(), 3);
        QVERIFY(!s.isEmpty());
    }

    void amplitudeAtValidIndex()
    {
        SweepData s(470e6, 1e6, {-80.0, -75.0, -90.0});
        QCOMPARE(s.amplitudeAt(0), -80.0);
        QCOMPARE(s.amplitudeAt(1), -75.0);
        QCOMPARE(s.amplitudeAt(2), -90.0);
    }

    void amplitudeAtOutOfBoundsReturnsNaN()
    {
        SweepData s(470e6, 1e6, {-80.0, -75.0});
        QVERIFY(std::isnan(s.amplitudeAt(-1)));
        QVERIFY(std::isnan(s.amplitudeAt(2)));
        QVERIFY(std::isnan(s.amplitudeAt(100)));
    }

    void minMaxOnEmptyReturnsNaN()
    {
        SweepData s;
        QVERIFY(std::isnan(s.minAmplitude()));
        QVERIFY(std::isnan(s.maxAmplitude()));
    }

    void timestampDefaultIsValid()
    {
        SweepData s(470e6, 1e6, {-80.0});
        QVERIFY(s.timestamp().isValid());
    }

    void timestampCustomValue()
    {
        QDateTime custom(QDate(2026, 1, 15), QTime(10, 30, 0), Qt::UTC);
        SweepData s(470e6, 1e6, {-80.0}, custom);
        QCOMPARE(s.timestamp(), custom);
    }

    void defaultConstructorValues()
    {
        SweepData s;
        QCOMPARE(s.startFreqHz(), 0.0);
        QCOMPARE(s.stepSizeHz(), 0.0);
        QCOMPARE(s.count(), 0);
        QVERIFY(s.isEmpty());
    }

    void singlePointSweep()
    {
        SweepData s(470e6, 1e6, {-80.0});
        QCOMPARE(s.count(), 1);
        QCOMPARE(s.stopFreqHz(), 470e6);
        QCOMPARE(s.minAmplitude(), -80.0);
        QCOMPARE(s.maxAmplitude(), -80.0);
    }

    void amplitudesVectorAccessor()
    {
        QVector<double> amps = {-80.0, -75.0, -90.0};
        SweepData s(470e6, 1e6, amps);
        QCOMPARE(s.amplitudes(), amps);
    }
};

QTEST_GUILESS_MAIN(tst_SweepData)
#include "tst_SweepData.moc"
