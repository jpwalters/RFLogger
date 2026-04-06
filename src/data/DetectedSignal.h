#pragma once

#include <QString>

struct DetectedSignal
{
    double centerFreqHz = 0.0;
    double peakAmplitudeDbm = -120.0;
    double bandwidthHz = 0.0;
    QString label;
};
