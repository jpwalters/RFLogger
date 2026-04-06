#pragma once

#include "data/SweepData.h"

#include <QObject>
#include <QString>

class ISpectrumDevice : public QObject
{
    Q_OBJECT

public:
    explicit ISpectrumDevice(QObject* parent = nullptr) : QObject(parent) {}
    virtual ~ISpectrumDevice() = default;

    virtual QString deviceName() const = 0;
    virtual QString deviceType() const = 0;

    virtual bool connectDevice(const QString& portOrPath) = 0;
    virtual void disconnectDevice() = 0;
    virtual bool isConnected() const = 0;

    virtual bool configure(double startFreqHz, double stopFreqHz, int sweepPoints) = 0;
    virtual bool startScanning() = 0;
    virtual void stopScanning() = 0;
    virtual bool isScanning() const = 0;

    virtual double minFreqHz() const = 0;
    virtual double maxFreqHz() const = 0;
    virtual int minSweepPoints() const = 0;
    virtual int maxSweepPoints() const = 0;

    virtual QString firmwareVersion() const { return {}; }
    virtual QString serialNumber() const { return {}; }
    virtual QString modelName() const { return deviceName(); }
    virtual bool supportsDemodulation() const { return false; }

signals:
    void sweepReady(const SweepData& sweep);
    void partialSweepReady(const SweepData& sweep);
    void sweepProgress(int percent);
    void connectionChanged(bool connected);
    void errorOccurred(const QString& message);
    void deviceInfoUpdated();
};
