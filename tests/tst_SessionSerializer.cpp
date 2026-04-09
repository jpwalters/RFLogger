#include <QTest>
#include <QTemporaryFile>
#include <QTemporaryDir>
#include <QTextStream>
#include <cmath>
#include "data/SessionSerializer.h"
#include "data/ScanSession.h"

class tst_SessionSerializer : public QObject
{
    Q_OBJECT

private slots:
    void saveAndLoadRoundTrip()
    {
        ScanSession session;
        session.setDeviceName("RF Explorer");
        session.setStartTime(QDateTime(QDate(2026, 4, 8), QTime(12, 0, 0), Qt::UTC));
        session.setStopTime(QDateTime(QDate(2026, 4, 8), QTime(12, 30, 0), Qt::UTC));

        session.addSweep(SweepData(470e6, 1e6, {-80.0, -75.0, -90.0}));
        session.addSweep(SweepData(470e6, 1e6, {-70.0, -85.0, -60.0}));
        session.addSweep(SweepData(470e6, 1e6, {-65.0, -72.0, -55.0}));

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QString filePath = dir.filePath("test_session.rfl");

        // Save
        QVERIFY(SessionSerializer::save(filePath, session));

        // Load
        auto result = SessionSerializer::load(filePath);
        QVERIFY(result.success);
        QCOMPARE(result.deviceName, "RF Explorer");
        QCOMPARE(result.sweepCount, 3);
        QVERIFY(result.startTime.isValid());
        QVERIFY(result.stopTime.isValid());

        // Check max-hold values
        QCOMPARE(result.maxHold.count(), 3);
        QCOMPARE(result.maxHold.startFreqHz(), 470e6);
        QCOMPARE(result.maxHold.stepSizeHz(), 1e6);
        QCOMPARE(result.maxHold.amplitudes()[0], -65.0);  // max(-80, -70, -65)
        QCOMPARE(result.maxHold.amplitudes()[1], -72.0);  // max(-75, -85, -72)
        QCOMPARE(result.maxHold.amplitudes()[2], -55.0);  // max(-90, -60, -55)
    }

    void saveEmptySessionFails()
    {
        ScanSession session;
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(!SessionSerializer::save(dir.filePath("empty.rfl"), session));
    }

    void loadNonexistentFileFails()
    {
        auto result = SessionSerializer::load("/nonexistent/path/file.rfl");
        QVERIFY(!result.success);
        QVERIFY(!result.errorMessage.isEmpty());
    }

    void loadInvalidJsonFails()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QString filePath = dir.filePath("bad.rfl");

        QFile file(filePath);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("not valid json{{{");
        file.close();

        auto result = SessionSerializer::load(filePath);
        QVERIFY(!result.success);
        QVERIFY(result.errorMessage.contains("parse error", Qt::CaseInsensitive));
    }

    void loadMissingVersionFails()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QString filePath = dir.filePath("noversion.rfl");

        QFile file(filePath);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(R"({"maxHold":{"startFreqHz":470000000,"stepSizeHz":1000000,"amplitudes":[-80]}})");
        file.close();

        auto result = SessionSerializer::load(filePath);
        QVERIFY(!result.success);
    }

    void averageTracePreserved()
    {
        ScanSession session;
        session.addSweep(SweepData(470e6, 1e6, {-30.0, -50.0}));
        session.addSweep(SweepData(470e6, 1e6, {-30.0, -50.0}));

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QString filePath = dir.filePath("avg.rfl");

        QVERIFY(SessionSerializer::save(filePath, session));

        auto result = SessionSerializer::load(filePath);
        QVERIFY(result.success);
        QCOMPARE(result.average.count(), 2);
        QVERIFY(qAbs(result.average.amplitudes()[0] - (-30.0)) < 0.01);
        QVERIFY(qAbs(result.average.amplitudes()[1] - (-50.0)) < 0.01);
    }

    void frequencyParamsPreserved()
    {
        ScanSession session;
        session.addSweep(SweepData(500e6, 0.5e6, {-80.0, -75.0, -90.0, -85.0}));

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QString filePath = dir.filePath("freq.rfl");

        QVERIFY(SessionSerializer::save(filePath, session));

        auto result = SessionSerializer::load(filePath);
        QVERIFY(result.success);
        QCOMPARE(result.maxHold.startFreqHz(), 500e6);
        QCOMPARE(result.maxHold.stepSizeHz(), 0.5e6);
        QCOMPARE(result.maxHold.count(), 4);
    }

    // ── CSV Import tests ─────────────────────────────────────────────────

    void loadCsvRoundTrip()
    {
        // Write a CSV file matching GenericCsvExporter format
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QString filePath = dir.filePath("scan.csv");

        QFile file(filePath);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
        QTextStream out(&file);
        out << "# RFLogger Export\n";
        out << "# Date: 2026-04-09T12:00:00\n";
        out << "# Device: RF Explorer\n";
        out << "# Start Freq (MHz): 470.000000\n";
        out << "# Stop Freq (MHz): 473.000000\n";
        out << "# Points: 4\n";
        out << "#\n";
        out << "Frequency (MHz),Amplitude (dBm)\n";
        out << "470.000000,-80.0\n";
        out << "471.000000,-75.5\n";
        out << "472.000000,-90.2\n";
        out << "473.000000,-65.0\n";
        file.close();

        auto result = SessionSerializer::loadCsv(filePath);
        QVERIFY(result.success);
        QCOMPARE(result.sweep.count(), 4);
        QVERIFY(qAbs(result.sweep.startFreqHz() - 470e6) < 1.0);
        QVERIFY(qAbs(result.sweep.amplitudeAt(0) - (-80.0)) < 0.01);
        QVERIFY(qAbs(result.sweep.amplitudeAt(1) - (-75.5)) < 0.01);
        QVERIFY(qAbs(result.sweep.amplitudeAt(3) - (-65.0)) < 0.01);
    }

    void loadCsvBareCsv()
    {
        // No comments, no header — just data (like Shure WWB format)
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QString filePath = dir.filePath("bare.csv");

        QFile file(filePath);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
        QTextStream out(&file);
        out << "470.000000,-80.0\n";
        out << "471.000000,-75.0\n";
        out << "472.000000,-70.0\n";
        file.close();

        auto result = SessionSerializer::loadCsv(filePath);
        QVERIFY(result.success);
        QCOMPARE(result.sweep.count(), 3);
    }

    void loadCsvTooFewPoints()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QString filePath = dir.filePath("tiny.csv");

        QFile file(filePath);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
        QTextStream out(&file);
        out << "Frequency (MHz),Amplitude (dBm)\n";
        out << "470.000000,-80.0\n";
        file.close();

        auto result = SessionSerializer::loadCsv(filePath);
        QVERIFY(!result.success);
        QVERIFY(result.errorMessage.contains("fewer than 2"));
    }

    void loadCsvNonexistentFails()
    {
        auto result = SessionSerializer::loadCsv("/does/not/exist.csv");
        QVERIFY(!result.success);
    }
};

QTEST_GUILESS_MAIN(tst_SessionSerializer)
#include "tst_SessionSerializer.moc"
