#include "PeakDetector.h"

#include <algorithm>
#include <cmath>

double PeakDetector::estimateNoiseFloor(const QVector<double>& amplitudes) const
{
    if (amplitudes.isEmpty())
        return -120.0;

    // Use the median as a robust noise floor estimate
    QVector<double> sorted = amplitudes;
    std::sort(sorted.begin(), sorted.end());
    return sorted[static_cast<int>(sorted.size()) / 2];
}

QVector<DetectedSignal> PeakDetector::detect(const SweepData& sweep) const
{
    QVector<DetectedSignal> detected;

    if (sweep.isEmpty())
        return detected;

    const auto& amps = sweep.amplitudes();
    const int n = static_cast<int>(amps.size());
    if (n < 3)
        return detected;

    double noiseFloor = estimateNoiseFloor(amps);
    double threshold = noiseFloor + m_thresholdDb;

    // Find local maxima above threshold
    struct RawPeak {
        int index;
        double amplitude;
    };
    QVector<RawPeak> peaks;

    for (int i = 1; i < n - 1; ++i) {
        if (amps[i] > threshold && amps[i] >= amps[i - 1] && amps[i] >= amps[i + 1]) {
            peaks.append({i, amps[i]});
        }
    }

    // Merge peaks that are closer than minSeparationHz
    // Sort by amplitude descending — keep the strongest peak in each group
    std::sort(peaks.begin(), peaks.end(), [](const RawPeak& a, const RawPeak& b) {
        return a.amplitude > b.amplitude;
    });

    QVector<bool> suppressed(peaks.size(), false);
    for (int i = 0; i < static_cast<int>(peaks.size()); ++i) {
        if (suppressed[i])
            continue;
        double freqI = sweep.frequencyAtIndex(peaks[i].index);
        for (int j = i + 1; j < static_cast<int>(peaks.size()); ++j) {
            if (suppressed[j])
                continue;
            double freqJ = sweep.frequencyAtIndex(peaks[j].index);
            if (std::abs(freqI - freqJ) < m_minSeparationHz)
                suppressed[j] = true;
        }
    }

    // Build detected signals with bandwidth estimation
    for (int i = 0; i < static_cast<int>(peaks.size()); ++i) {
        if (suppressed[i])
            continue;

        int peakIdx = peaks[i].index;
        double peakAmp = peaks[i].amplitude;

        // Estimate -3 dB bandwidth
        double bwThreshold = peakAmp - 3.0;

        int left = peakIdx;
        while (left > 0 && amps[left - 1] >= bwThreshold)
            --left;

        int right = peakIdx;
        while (right < n - 1 && amps[right + 1] >= bwThreshold)
            ++right;

        double leftFreq = sweep.frequencyAtIndex(left);
        double rightFreq = sweep.frequencyAtIndex(right);

        DetectedSignal sig;
        sig.centerFreqHz = sweep.frequencyAtIndex(peakIdx);
        sig.peakAmplitudeDbm = peakAmp;
        sig.bandwidthHz = rightFreq - leftFreq;
        sig.label = QString("%1 MHz").arg(sig.centerFreqHz / 1e6, 0, 'f', 3);

        detected.append(sig);
    }

    // Sort by frequency ascending for display
    std::sort(detected.begin(), detected.end(), [](const DetectedSignal& a, const DetectedSignal& b) {
        return a.centerFreqHz < b.centerFreqHz;
    });

    return detected;
}
