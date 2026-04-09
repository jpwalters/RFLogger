#include "ScanSession.h"

#include <algorithm>
#include <cmath>
#include <limits>

ScanSession::ScanSession(QObject* parent)
    : QObject(parent)
{
}

void ScanSession::addSweep(const SweepData& sweep)
{
    const int count = sweep.count();

    // Initialize or reset accumulators when sweep point count changes
    if (m_maxHoldAmplitudes.size() != count) {
        if (m_totalSweepCount > 0)
            qDebug() << "ScanSession: sweep point count changed from"
                     << m_maxHoldAmplitudes.size() << "to" << count
                     << "\u2014 resetting accumulators";
        m_maxHoldAmplitudes.resize(count);
        m_sumAmplitudes.resize(count);
        std::fill(m_maxHoldAmplitudes.begin(), m_maxHoldAmplitudes.end(),
                  -std::numeric_limits<double>::infinity());
        std::fill(m_sumAmplitudes.begin(), m_sumAmplitudes.end(), 0.0);
        m_totalSweepCount = 0;
    }

    // Store reference frequency from the first sweep (or after reset)
    if (m_totalSweepCount == 0) {
        m_refStartFreqHz = sweep.startFreqHz();
        m_refStepSizeHz = sweep.stepSizeHz();
    }

    const auto& amps = sweep.amplitudes();
    for (int i = 0; i < count && i < static_cast<int>(amps.size()); ++i) {
        m_maxHoldAmplitudes[i] = std::max(m_maxHoldAmplitudes[i], amps[i]);
        // Accumulate in linear power domain so the average is correct.
        // Summing dBm directly would underestimate the true mean power.
        m_sumAmplitudes[i] += std::pow(10.0, amps[i] / 10.0);
    }

    m_sweeps.append(sweep);
    ++m_totalSweepCount;

    // Evict oldest sweeps to bound memory usage
    if (m_sweeps.size() > MAX_STORED_SWEEPS)
        m_sweeps.removeFirst();

    emit sweepAdded(sweep);
}

void ScanSession::clear()
{
    m_sweeps.clear();
    m_maxHoldAmplitudes.clear();
    m_sumAmplitudes.clear();
    m_totalSweepCount = 0;
    m_refStartFreqHz = 0.0;
    m_refStepSizeHz = 0.0;
    m_startTime = QDateTime();
    m_stopTime = QDateTime();
    emit sessionCleared();
}

const SweepData& ScanSession::latestSweep() const
{
    static const SweepData empty;
    if (m_sweeps.isEmpty())
        return empty;
    return m_sweeps.last();
}

SweepData ScanSession::maxHold() const
{
    if (m_totalSweepCount == 0)
        return {};

    return SweepData(m_refStartFreqHz, m_refStepSizeHz, m_maxHoldAmplitudes);
}

SweepData ScanSession::average() const
{
    if (m_totalSweepCount == 0)
        return {};

    QVector<double> avgAmps(m_sumAmplitudes.size());
    for (int i = 0; i < static_cast<int>(avgAmps.size()); ++i) {
        // Convert mean linear power back to dBm
        avgAmps[i] = 10.0 * std::log10(m_sumAmplitudes[i] / m_totalSweepCount);
    }

    return SweepData(m_refStartFreqHz, m_refStepSizeHz, avgAmps);
}
