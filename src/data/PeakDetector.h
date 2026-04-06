#pragma once

#include "DetectedSignal.h"
#include "SweepData.h"

#include <QVector>

class PeakDetector
{
public:
    PeakDetector() = default;

    void setThresholdDb(double db) { m_thresholdDb = db; }
    double thresholdDb() const { return m_thresholdDb; }

    void setMinSeparationHz(double hz) { m_minSeparationHz = hz; }
    double minSeparationHz() const { return m_minSeparationHz; }

    QVector<DetectedSignal> detect(const SweepData& sweep) const;

private:
    double estimateNoiseFloor(const QVector<double>& amplitudes) const;

    double m_thresholdDb = 10.0;
    double m_minSeparationHz = 25000.0; // 25 kHz default minimum separation
};
