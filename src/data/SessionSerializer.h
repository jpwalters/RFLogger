#pragma once

#include "ScanSession.h"
#include "SweepData.h"

#include <QString>

class SessionSerializer
{
public:
    // Save session max-hold and average traces to a JSON file.
    static bool save(const QString& filePath, const ScanSession& session);

    // Load a previously saved session file. Returns a SweepData representing
    // the max-hold trace, which is the primary data used for reference overlays.
    struct LoadResult {
        bool success = false;
        QString errorMessage;
        SweepData maxHold;
        SweepData average;
        QString deviceName;
        QDateTime startTime;
        QDateTime stopTime;
        int sweepCount = 0;
    };

    static LoadResult load(const QString& filePath);

    // Import a CSV file exported by GenericCsvExporter (or compatible format).
    // Expects lines of "Frequency (MHz),Amplitude (dBm)" with optional # comments.
    struct CsvImportResult {
        bool success = false;
        QString errorMessage;
        SweepData sweep;
    };

    static CsvImportResult loadCsv(const QString& filePath);
};
