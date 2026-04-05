#include "WwbExporter.h"

#include <QFile>
#include <QTextStream>

bool WwbExporter::exportToFile(const QString& filePath, const SweepData& sweep)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    QTextStream out(&file);
    out << formatSweep(sweep);
    return true;
}

QString WwbExporter::formatSweep(const SweepData& sweep)
{
    // Shure Wireless Workbench format:
    // 2-column CSV, NO header row
    // Column 1: frequency in MHz (6 decimal places)
    // Column 2: amplitude in dBm (1 decimal place)
    QString result;
    result.reserve(sweep.count() * 30);

    for (int i = 0; i < sweep.count(); ++i) {
        double freqMhz = sweep.frequencyAtIndex(i) / 1'000'000.0;
        double ampDbm = sweep.amplitudeAt(i);
        result += QString::number(freqMhz, 'f', 6);
        result += ',';
        result += QString::number(ampDbm, 'f', 1);
        result += '\n';
    }

    return result;
}
