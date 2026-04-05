#pragma once

#include "data/SweepData.h"

#include <QString>

class WwbExporter
{
public:
    static bool exportToFile(const QString& filePath, const SweepData& sweep);
    static QString formatSweep(const SweepData& sweep);
};
