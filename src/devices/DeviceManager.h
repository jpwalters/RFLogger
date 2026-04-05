#pragma once

#include "ISpectrumDevice.h"

#include <QSerialPortInfo>
#include <QTimer>
#include <QVector>
#include <memory>

class DeviceManager : public QObject
{
    Q_OBJECT

public:
    enum class DeviceType {
        AutoDetect,
        RFExplorer,
        TinySA,
        RtlSdr
    };
    Q_ENUM(DeviceType)

    explicit DeviceManager(QObject* parent = nullptr);
    ~DeviceManager() override;

    QStringList availablePorts() const;
    QStringList availableRtlSdrDevices() const;

    bool connectDevice(const QString& portOrPath, DeviceType type = DeviceType::AutoDetect);
    void disconnectDevice();

    ISpectrumDevice* currentDevice() const { return m_currentDevice; }
    bool isConnected() const { return m_currentDevice && m_currentDevice->isConnected(); }

    void refreshPorts();

signals:
    void portsChanged(const QStringList& ports);
    void deviceConnected(ISpectrumDevice* device);
    void deviceDisconnected();
    void errorOccurred(const QString& message);

private:
    DeviceType detectDeviceType(const QString& portName) const;

    ISpectrumDevice* m_currentDevice = nullptr;
    QTimer* m_pollTimer = nullptr;
    QStringList m_lastPorts;
};
