#include <QTest>
#include <QTemporaryFile>
#include <QTextStream>
#include "data/SweepData.h"
#include "export/GenericCsvExporter.h"

class tst_GenericCsvExporter : public QObject
{
    Q_OBJECT

private slots:
    void exportContainsHeaderRow()
    {
        SweepData s(470e6, 1e6, {-80.0, -75.0});

        QTemporaryFile file;
        file.setAutoRemove(true);
        QVERIFY(file.open());
        QString path = file.fileName();
        file.close();

        QVERIFY(GenericCsvExporter::exportToFile(path, s));

        QFile readBack(path);
        QVERIFY(readBack.open(QIODevice::ReadOnly));
        QString content = readBack.readAll();
        QVERIFY(content.contains("Frequency (MHz),Amplitude (dBm)"));
    }

    void exportContainsMetadataComments()
    {
        SweepData s(470e6, 1e6, {-80.0});

        QTemporaryFile file;
        file.setAutoRemove(true);
        QVERIFY(file.open());
        QString path = file.fileName();
        file.close();

        QVERIFY(GenericCsvExporter::exportToFile(path, s));

        QFile readBack(path);
        QVERIFY(readBack.open(QIODevice::ReadOnly));
        QString content = readBack.readAll();
        QVERIFY(content.contains("# RFLogger Export"));
        QVERIFY(content.contains("# Date:"));
        QVERIFY(content.contains("# Start Freq (MHz): 470.000000"));
        QVERIFY(content.contains("# Stop Freq (MHz): 470.000000"));
        QVERIFY(content.contains("# Points: 1"));
    }

    void exportIncludesDeviceName()
    {
        SweepData s(470e6, 1e6, {-80.0});

        QTemporaryFile file;
        file.setAutoRemove(true);
        QVERIFY(file.open());
        QString path = file.fileName();
        file.close();

        QVERIFY(GenericCsvExporter::exportToFile(path, s, "RF Explorer"));

        QFile readBack(path);
        QVERIFY(readBack.open(QIODevice::ReadOnly));
        QString content = readBack.readAll();
        QVERIFY(content.contains("# Device: RF Explorer"));
    }

    void exportIncludesSessionInfo()
    {
        SweepData s(470e6, 1e6, {-80.0});

        QTemporaryFile file;
        file.setAutoRemove(true);
        QVERIFY(file.open());
        QString path = file.fileName();
        file.close();

        QVERIFY(GenericCsvExporter::exportToFile(path, s, {}, "Session 1"));

        QFile readBack(path);
        QVERIFY(readBack.open(QIODevice::ReadOnly));
        QString content = readBack.readAll();
        QVERIFY(content.contains("# Session: Session 1"));
    }

    void exportOmitsEmptyOptionalFields()
    {
        SweepData s(470e6, 1e6, {-80.0});

        QTemporaryFile file;
        file.setAutoRemove(true);
        QVERIFY(file.open());
        QString path = file.fileName();
        file.close();

        QVERIFY(GenericCsvExporter::exportToFile(path, s));

        QFile readBack(path);
        QVERIFY(readBack.open(QIODevice::ReadOnly));
        QString content = readBack.readAll();
        QVERIFY(!content.contains("# Device:"));
        QVERIFY(!content.contains("# Session:"));
    }

    void exportFormatsDataCorrectly()
    {
        SweepData s(470e6, 0.5e6, {-85.5, -72.3, -100.0});

        QTemporaryFile file;
        file.setAutoRemove(true);
        QVERIFY(file.open());
        QString path = file.fileName();
        file.close();

        QVERIFY(GenericCsvExporter::exportToFile(path, s));

        QFile readBack(path);
        QVERIFY(readBack.open(QIODevice::ReadOnly));
        QString content = readBack.readAll();
        QVERIFY(content.contains("470.000000,-85.5"));
        QVERIFY(content.contains("470.500000,-72.3"));
        QVERIFY(content.contains("471.000000,-100.0"));
    }

    void exportEmptySweepProducesHeaderOnly()
    {
        SweepData s;

        QTemporaryFile file;
        file.setAutoRemove(true);
        QVERIFY(file.open());
        QString path = file.fileName();
        file.close();

        QVERIFY(GenericCsvExporter::exportToFile(path, s));

        QFile readBack(path);
        QVERIFY(readBack.open(QIODevice::ReadOnly));
        QString content = readBack.readAll();
        QVERIFY(content.contains("Frequency (MHz),Amplitude (dBm)"));
        // No data lines after header
        QStringList lines = content.split('\n', Qt::SkipEmptyParts);
        // Last non-empty line should be the header row
        QVERIFY(lines.last().trimmed() == "Frequency (MHz),Amplitude (dBm)");
    }

    void exportFailsForInvalidPath()
    {
        SweepData s(470e6, 1e6, {-80.0});
        QVERIFY(!GenericCsvExporter::exportToFile("Z:/nonexistent/path/file.csv", s));
    }
};

QTEST_GUILESS_MAIN(tst_GenericCsvExporter)
#include "tst_GenericCsvExporter.moc"
