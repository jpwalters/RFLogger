#pragma once

#include "SweepData.h"

#include <QDateTime>
#include <QObject>
#include <QString>
#include <QVector>

class ScanSession : public QObject
{
    Q_OBJECT

public:
    explicit ScanSession(QObject* parent = nullptr);

    void addSweep(const SweepData& sweep);
    void clear();

    const SweepData& latestSweep() const;
    SweepData maxHold() const;
    SweepData average() const;

    int sweepCount() const { return m_sweeps.size(); }
    bool isEmpty() const { return m_sweeps.isEmpty(); }

    const QVector<SweepData>& sweeps() const { return m_sweeps; }

    QString deviceName() const { return m_deviceName; }
    void setDeviceName(const QString& name) { m_deviceName = name; }

    QDateTime startTime() const { return m_startTime; }
    QDateTime stopTime() const { return m_stopTime; }
    void setStartTime(const QDateTime& t) { m_startTime = t; }
    void setStopTime(const QDateTime& t) { m_stopTime = t; }

signals:
    void sweepAdded(const SweepData& sweep);
    void sessionCleared();

private:
    QVector<SweepData> m_sweeps;
    QString m_deviceName;
    QDateTime m_startTime;
    QDateTime m_stopTime;

    // Running max-hold and average accumulators
    QVector<double> m_maxHoldAmplitudes;
    QVector<double> m_sumAmplitudes;
};
