#pragma once

#include <QDateTime>
#include <QVector>

class SweepData
{
public:
    SweepData() = default;
    SweepData(double startFreqHz, double stepSizeHz,
              QVector<double> amplitudesDbm, QDateTime timestamp = QDateTime::currentDateTimeUtc());

    double startFreqHz() const { return m_startFreqHz; }
    double stepSizeHz() const { return m_stepSizeHz; }
    double stopFreqHz() const;
    double frequencyAtIndex(int index) const;
    int count() const { return static_cast<int>(m_amplitudes.size()); }
    bool isEmpty() const { return m_amplitudes.isEmpty(); }

    const QVector<double>& amplitudes() const { return m_amplitudes; }
    double amplitudeAt(int index) const;
    double minAmplitude() const;
    double maxAmplitude() const;

    QDateTime timestamp() const { return m_timestamp; }

private:
    double m_startFreqHz = 0.0;
    double m_stepSizeHz = 0.0;
    QVector<double> m_amplitudes;
    QDateTime m_timestamp;
};
