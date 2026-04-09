#include <QTest>
#include <QTemporaryFile>
#include <QTextStream>
#include <cmath>
#include <limits>
#include "data/SweepData.h"
#include "export/WwbExporter.h"

class tst_WwbExporter : public QObject
{
    Q_OBJECT

private slots:
    void formatsSweepCorrectly()
    {
        SweepData s(470e6, 0.5e6, {-85.5, -72.3, -100.0});

        QString csv = WwbExporter::formatSweep(s);
        QStringList lines = csv.split('\n', Qt::SkipEmptyParts);

        QCOMPARE(lines.size(), 3);
        QCOMPARE(lines[0], "470.000000,-85.5");
        QCOMPARE(lines[1], "470.500000,-72.3");
        QCOMPARE(lines[2], "471.000000,-100.0");
    }

    void noHeaderRow()
    {
        SweepData s(470e6, 1e6, {-80.0});

        QString csv = WwbExporter::formatSweep(s);
        // First line should be data, not a header
        QVERIFY(csv.startsWith("470."));
    }

    void exportToFileWorks()
    {
        SweepData s(470e6, 1e6, {-80.0, -75.0});

        QTemporaryFile file;
        file.setAutoRemove(true);
        QVERIFY(file.open());
        QString path = file.fileName();
        file.close();

        QVERIFY(WwbExporter::exportToFile(path, s));

        QFile readBack(path);
        QVERIFY(readBack.open(QIODevice::ReadOnly));
        QString content = readBack.readAll();
        QVERIFY(content.contains("470.000000,-80.0"));
        QVERIFY(content.contains("471.000000,-75.0"));
    }

    void formatEmptySweep()
    {
        SweepData s;
        QString csv = WwbExporter::formatSweep(s);
        QVERIFY(csv.isEmpty());
    }

    void formatSinglePoint()
    {
        SweepData s(470e6, 1e6, {-80.0});
        QString csv = WwbExporter::formatSweep(s);
        QStringList lines = csv.split('\n', Qt::SkipEmptyParts);
        QCOMPARE(lines.size(), 1);
        QCOMPARE(lines[0], "470.000000,-80.0");
    }

    void exportFailsForInvalidPath()
    {
        SweepData s(470e6, 1e6, {-80.0});
        QVERIFY(!WwbExporter::exportToFile("Z:/nonexistent/path/file.csv", s));
    }

    void formatSweepWithNaNAmplitude()
    {
        double nan = std::numeric_limits<double>::quiet_NaN();
        SweepData s(470e6, 1e6, {-80.0, nan, -60.0});

        QString csv = WwbExporter::formatSweep(s);
        QStringList lines = csv.split('\n', Qt::SkipEmptyParts);
        QCOMPARE(lines.size(), 3);
        // NaN should be formatted as "nan" — verify no crash
        QVERIFY(!lines[1].isEmpty());
    }

    void formatSweepWithInfAmplitude()
    {
        double inf = std::numeric_limits<double>::infinity();
        SweepData s(470e6, 1e6, {-inf, -80.0, inf});

        QString csv = WwbExporter::formatSweep(s);
        QStringList lines = csv.split('\n', Qt::SkipEmptyParts);
        QCOMPARE(lines.size(), 3);
        // Verify no crash with infinite values
        QVERIFY(!lines[0].isEmpty());
        QVERIFY(!lines[2].isEmpty());
    }
};

QTEST_GUILESS_MAIN(tst_WwbExporter)
#include "tst_WwbExporter.moc"
