#include "SessionSerializer.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>

bool SessionSerializer::save(const QString& filePath, const ScanSession& session)
{
    if (session.isEmpty())
        return false;

    QJsonObject root;
    root["version"] = 1;
    root["deviceName"] = session.deviceName();
    root["sweepCount"] = session.sweepCount();

    if (session.startTime().isValid())
        root["startTime"] = session.startTime().toString(Qt::ISODate);
    if (session.stopTime().isValid())
        root["stopTime"] = session.stopTime().toString(Qt::ISODate);

    auto writeSweep = [](const SweepData& sweep) -> QJsonObject {
        QJsonObject obj;
        obj["startFreqHz"] = sweep.startFreqHz();
        obj["stepSizeHz"] = sweep.stepSizeHz();

        QJsonArray amps;
        for (double a : sweep.amplitudes())
            amps.append(a);
        obj["amplitudes"] = amps;

        if (sweep.timestamp().isValid())
            obj["timestamp"] = sweep.timestamp().toString(Qt::ISODate);

        return obj;
    };

    root["maxHold"] = writeSweep(session.maxHold());
    root["average"] = writeSweep(session.average());

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    return true;
}

SessionSerializer::LoadResult SessionSerializer::load(const QString& filePath)
{
    LoadResult result;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        result.errorMessage = QStringLiteral("Cannot open file: %1").arg(filePath);
        return result;
    }

    // Limit file size to 50 MB to prevent resource exhaustion
    constexpr qint64 MAX_FILE_SIZE = 50 * 1024 * 1024;
    if (file.size() > MAX_FILE_SIZE) {
        result.errorMessage = QStringLiteral("File exceeds maximum allowed size (50 MB)");
        return result;
    }

    QJsonParseError parseError;
    auto doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (doc.isNull()) {
        result.errorMessage = QStringLiteral("JSON parse error: %1").arg(parseError.errorString());
        return result;
    }

    QJsonObject root = doc.object();
    int version = root.value("version").toInt(0);
    if (version < 1) {
        result.errorMessage = QStringLiteral("Unknown file format version");
        return result;
    }

    auto readSweep = [](const QJsonObject& obj) -> SweepData {
        double startFreqHz = obj.value("startFreqHz").toDouble();
        double stepSizeHz = obj.value("stepSizeHz").toDouble();

        QJsonArray ampsArray = obj.value("amplitudes").toArray();
        QVector<double> amps;
        amps.reserve(ampsArray.size());
        for (const auto& v : ampsArray)
            amps.append(v.toDouble());

        QDateTime ts;
        if (obj.contains("timestamp"))
            ts = QDateTime::fromString(obj.value("timestamp").toString(), Qt::ISODate);

        return SweepData(startFreqHz, stepSizeHz, std::move(amps), ts);
    };

    result.maxHold = readSweep(root.value("maxHold").toObject());
    result.average = readSweep(root.value("average").toObject());
    result.deviceName = root.value("deviceName").toString();
    result.sweepCount = root.value("sweepCount").toInt();

    if (root.contains("startTime"))
        result.startTime = QDateTime::fromString(root.value("startTime").toString(), Qt::ISODate);
    if (root.contains("stopTime"))
        result.stopTime = QDateTime::fromString(root.value("stopTime").toString(), Qt::ISODate);

    if (result.maxHold.isEmpty()) {
        result.errorMessage = QStringLiteral("File contains no max-hold data");
        return result;
    }

    result.success = true;
    return result;
}

SessionSerializer::CsvImportResult SessionSerializer::loadCsv(const QString& filePath)
{
    CsvImportResult result;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        result.errorMessage = QStringLiteral("Cannot open file: %1").arg(filePath);
        return result;
    }

    constexpr qint64 MAX_FILE_SIZE = 50 * 1024 * 1024;
    if (file.size() > MAX_FILE_SIZE) {
        result.errorMessage = QStringLiteral("File exceeds maximum allowed size (50 MB)");
        return result;
    }

    QTextStream in(&file);
    QVector<double> freqsHz;
    QVector<double> ampsDbm;
    bool headerSeen = false;

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty())
            continue;

        // Skip comment lines (metadata)
        if (line.startsWith('#'))
            continue;

        // Skip header row
        if (!headerSeen && line.contains("Frequency", Qt::CaseInsensitive)) {
            headerSeen = true;
            continue;
        }

        // Parse data: "freq,amp"
        QStringList parts = line.split(',');
        if (parts.size() < 2)
            continue;

        bool freqOk = false, ampOk = false;
        double freqMHz = parts[0].trimmed().toDouble(&freqOk);
        double ampDbm = parts[1].trimmed().toDouble(&ampOk);

        if (!freqOk || !ampOk)
            continue;

        freqsHz.append(freqMHz * 1'000'000.0);
        ampsDbm.append(ampDbm);
    }

    if (freqsHz.size() < 2) {
        result.errorMessage = QStringLiteral("CSV contains fewer than 2 data points");
        return result;
    }

    double startFreqHz = freqsHz.first();
    double stepSizeHz = (freqsHz.last() - freqsHz.first()) / (freqsHz.size() - 1);

    result.sweep = SweepData(startFreqHz, stepSizeHz, std::move(ampsDbm));
    result.success = true;
    return result;
}
