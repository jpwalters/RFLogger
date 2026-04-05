#pragma once

#include "data/SweepData.h"

#include <QString>

class GenericCsvExporter
{
public:
    static bool exportToFile(const QString& filePath, const SweepData& sweep,
                             const QString& deviceName = {},
                             const QString& sessionInfo = {});
};
