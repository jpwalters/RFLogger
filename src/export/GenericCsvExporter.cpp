#include "GenericCsvExporter.h"

#include <QFile>
#include <QDateTime>
#include <QStringConverter>
#include <QTextStream>

bool GenericCsvExporter::exportToFile(const QString& filePath, const SweepData& sweep,
                                      const QString& deviceName, const QString& sessionInfo)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);

    // Metadata comments
    out << "# RFLogger Export\n";
    out << "# Date: " << QDateTime::currentDateTime().toString(Qt::ISODate) << '\n';
    if (!deviceName.isEmpty())
        out << "# Device: " << deviceName << '\n';
    if (!sessionInfo.isEmpty())
        out << "# Session: " << sessionInfo << '\n';
    out << "# Start Freq (MHz): " << QString::number(sweep.startFreqHz() / 1e6, 'f', 6) << '\n';
    out << "# Stop Freq (MHz): " << QString::number(sweep.stopFreqHz() / 1e6, 'f', 6) << '\n';
    out << "# Points: " << sweep.count() << '\n';
    out << "#\n";

    // Header row
    out << "Frequency (MHz),Amplitude (dBm)\n";

    // Data rows
    for (int i = 0; i < sweep.count(); ++i) {
        double freqMhz = sweep.frequencyAtIndex(i) / 1'000'000.0;
        double ampDbm = sweep.amplitudeAt(i);
        out << QString::number(freqMhz, 'f', 6) << ',' << QString::number(ampDbm, 'f', 1) << '\n';
    }

    out.flush();
    if (file.error() != QFileDevice::NoError)
        return false;

    file.close();
    return file.error() == QFileDevice::NoError;
}
