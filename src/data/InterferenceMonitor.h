#pragma once

#include "SweepData.h"

#include <QMetaType>
#include <QObject>
#include <QString>
#include <QVector>

struct InterferenceAlert
{
    double frequencyHz = 0.0;
    double amplitudeDbm = -120.0;
    double thresholdDbm = -120.0;
    QString description;
};

Q_DECLARE_METATYPE(InterferenceAlert)

// Monitors specific frequency ranges for new interference.
// Compare current sweep against a baseline (typically the reference/max-hold trace)
// and emit alerts when activity rises above a configurable threshold.
class InterferenceMonitor : public QObject
{
    Q_OBJECT

public:
    explicit InterferenceMonitor(QObject* parent = nullptr);

    void setEnabled(bool enabled);
    bool isEnabled() const { return m_enabled; }

    // Set the reference baseline to compare against.
    // Alerts trigger when the live signal exceeds baseline + threshold.
    void setBaseline(const SweepData& baseline);
    bool hasBaseline() const { return !m_baseline.isEmpty(); }

    // Threshold in dB above baseline to trigger an alert.
    void setThresholdDb(double db) { m_thresholdDb = db; }
    double thresholdDb() const { return m_thresholdDb; }

    // Minimum interval between alert signals (prevents alert fatigue)
    void setCooldownMs(int ms) { m_cooldownMs = ms; }

    // Specific frequencies to watch. If empty, monitors the entire sweep.
    void setWatchFrequencies(const QVector<double>& freqsHz, double toleranceHz = 50000.0);
    void clearWatchFrequencies();

    // Call on each incoming sweep to check for interference
    void checkSweep(const SweepData& sweep);

signals:
    void alertTriggered(const InterferenceAlert& alert);
    void alertsCleared();

private:
    bool m_enabled = false;
    SweepData m_baseline;
    double m_thresholdDb = 6.0;
    int m_cooldownMs = 3000;
    qint64 m_lastAlertTime = 0;

    struct WatchEntry {
        double centerHz;
        double toleranceHz;
    };
    QVector<WatchEntry> m_watchFrequencies;
};
