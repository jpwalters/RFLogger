#include "ScanSession.h"

#include <algorithm>
#include <limits>

ScanSession::ScanSession(QObject* parent)
    : QObject(parent)
{
}

void ScanSession::addSweep(const SweepData& sweep)
{
    const int count = sweep.count();

    // Initialize or update accumulators
    if (m_maxHoldAmplitudes.size() != count) {
        m_maxHoldAmplitudes.resize(count);
        m_sumAmplitudes.resize(count);
        std::fill(m_maxHoldAmplitudes.begin(), m_maxHoldAmplitudes.end(),
                  -std::numeric_limits<double>::infinity());
        std::fill(m_sumAmplitudes.begin(), m_sumAmplitudes.end(), 0.0);
    }

    const auto& amps = sweep.amplitudes();
    for (int i = 0; i < count && i < amps.size(); ++i) {
        m_maxHoldAmplitudes[i] = std::max(m_maxHoldAmplitudes[i], amps[i]);
        m_sumAmplitudes[i] += amps[i];
    }

    m_sweeps.append(sweep);
    emit sweepAdded(sweep);
}

void ScanSession::clear()
{
    m_sweeps.clear();
    m_maxHoldAmplitudes.clear();
    m_sumAmplitudes.clear();
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
    if (m_sweeps.isEmpty())
        return {};

    const auto& ref = m_sweeps.first();
    return SweepData(ref.startFreqHz(), ref.stepSizeHz(), m_maxHoldAmplitudes);
}

SweepData ScanSession::average() const
{
    if (m_sweeps.isEmpty())
        return {};

    const int n = m_sweeps.size();
    QVector<double> avgAmps(m_sumAmplitudes.size());
    for (int i = 0; i < avgAmps.size(); ++i) {
        avgAmps[i] = m_sumAmplitudes[i] / n;
    }

    const auto& ref = m_sweeps.first();
    return SweepData(ref.startFreqHz(), ref.stepSizeHz(), avgAmps);
}
