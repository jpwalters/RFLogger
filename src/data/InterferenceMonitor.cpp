#include "InterferenceMonitor.h"

#include <QDateTime>
#include <cmath>

InterferenceMonitor::InterferenceMonitor(QObject* parent)
    : QObject(parent)
{
}

void InterferenceMonitor::setEnabled(bool enabled)
{
    m_enabled = enabled;
    if (!enabled)
        emit alertsCleared();
}

void InterferenceMonitor::setBaseline(const SweepData& baseline)
{
    m_baseline = baseline;
}

void InterferenceMonitor::setWatchFrequencies(const QVector<double>& freqsHz, double toleranceHz)
{
    m_watchFrequencies.clear();
    m_watchFrequencies.reserve(freqsHz.size());
    for (double f : freqsHz)
        m_watchFrequencies.append({f, toleranceHz});
}

void InterferenceMonitor::clearWatchFrequencies()
{
    m_watchFrequencies.clear();
}

void InterferenceMonitor::checkSweep(const SweepData& sweep)
{
    if (!m_enabled || m_baseline.isEmpty() || sweep.isEmpty())
        return;

    // Cooldown: avoid alert fatigue
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - m_lastAlertTime < m_cooldownMs)
        return;

    const auto& liveAmps = sweep.amplitudes();
    const auto& baseAmps = m_baseline.amplitudes();
    const int n = std::min(liveAmps.size(), baseAmps.size());

    if (n == 0)
        return;

    // If watching specific frequencies, only check those
    if (!m_watchFrequencies.isEmpty()) {
        for (const auto& watch : m_watchFrequencies) {
            // Find the sweep index closest to this frequency
            double freqDiff = watch.centerHz - sweep.startFreqHz();
            if (sweep.stepSizeHz() <= 0)
                continue;
            int centerIdx = static_cast<int>(std::round(freqDiff / sweep.stepSizeHz()));
            int toleranceIdxRange = static_cast<int>(std::ceil(watch.toleranceHz / sweep.stepSizeHz()));

            int startIdx = std::max(0, centerIdx - toleranceIdxRange);
            int endIdx = std::min(n - 1, centerIdx + toleranceIdxRange);

            double worstExceedance = 0.0;
            int worstIdx = -1;
            for (int i = startIdx; i <= endIdx; ++i) {
                double exceedance = liveAmps[i] - baseAmps[i];
                if (exceedance > m_thresholdDb && exceedance > worstExceedance) {
                    worstExceedance = exceedance;
                    worstIdx = i;
                }
            }

            if (worstIdx >= 0) {
                InterferenceAlert alert;
                alert.frequencyHz = sweep.frequencyAtIndex(worstIdx);
                alert.amplitudeDbm = liveAmps[worstIdx];
                alert.thresholdDbm = baseAmps[worstIdx] + m_thresholdDb;
                alert.description = QString("Interference at %1 MHz: %2 dBm (baseline + %3 dB)")
                                        .arg(alert.frequencyHz / 1e6, 0, 'f', 3)
                                        .arg(alert.amplitudeDbm, 0, 'f', 1)
                                        .arg(worstExceedance, 0, 'f', 1);
                m_lastAlertTime = now;
                emit alertTriggered(alert);
                return; // Only one alert per cooldown
            }
        }
        return;
    }

    // Full-sweep monitoring: find the worst exceedance
    double worstExceedance = 0.0;
    int worstIdx = -1;
    for (int i = 0; i < n; ++i) {
        double exceedance = liveAmps[i] - baseAmps[i];
        if (exceedance > m_thresholdDb && exceedance > worstExceedance) {
            worstExceedance = exceedance;
            worstIdx = i;
        }
    }

    if (worstIdx >= 0) {
        InterferenceAlert alert;
        alert.frequencyHz = sweep.frequencyAtIndex(worstIdx);
        alert.amplitudeDbm = liveAmps[worstIdx];
        alert.thresholdDbm = m_baseline.amplitudeAt(worstIdx) + m_thresholdDb;
        alert.description = QString("New interference at %1 MHz: %2 dBm (+%3 dB above baseline)")
                                .arg(alert.frequencyHz / 1e6, 0, 'f', 3)
                                .arg(alert.amplitudeDbm, 0, 'f', 1)
                                .arg(worstExceedance, 0, 'f', 1);
        m_lastAlertTime = now;
        emit alertTriggered(alert);
    }
}
