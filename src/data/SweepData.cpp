#include "SweepData.h"

#include <algorithm>
#include <limits>

SweepData::SweepData(double startFreqHz, double stepSizeHz,
                     QVector<double> amplitudesDbm, QDateTime timestamp)
    : m_startFreqHz(startFreqHz)
    , m_stepSizeHz(stepSizeHz)
    , m_amplitudes(std::move(amplitudesDbm))
    , m_timestamp(timestamp)
{
}

double SweepData::stopFreqHz() const
{
    if (m_amplitudes.isEmpty())
        return m_startFreqHz;
    return m_startFreqHz + m_stepSizeHz * (m_amplitudes.size() - 1);
}

double SweepData::frequencyAtIndex(int index) const
{
    return m_startFreqHz + m_stepSizeHz * index;
}

double SweepData::amplitudeAt(int index) const
{
    if (index < 0 || index >= m_amplitudes.size())
        return std::numeric_limits<double>::quiet_NaN();
    return m_amplitudes[index];
}

double SweepData::minAmplitude() const
{
    if (m_amplitudes.isEmpty())
        return std::numeric_limits<double>::quiet_NaN();
    return *std::min_element(m_amplitudes.begin(), m_amplitudes.end());
}

double SweepData::maxAmplitude() const
{
    if (m_amplitudes.isEmpty())
        return std::numeric_limits<double>::quiet_NaN();
    return *std::max_element(m_amplitudes.begin(), m_amplitudes.end());
}
