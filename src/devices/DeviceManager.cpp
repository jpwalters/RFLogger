#include "DeviceManager.h"
#include "RFExplorerDevice.h"
#include "TinySADevice.h"

#ifdef RFLOGGER_HAS_RTLSDR
#include "RtlSdrDevice.h"
#endif

#include <QSerialPortInfo>
#include <QDebug>

DeviceManager::DeviceManager(QObject* parent)
    : QObject(parent)
    , m_pollTimer(new QTimer(this))
{
    // Poll for USB hotplug every 2 seconds
    connect(m_pollTimer, &QTimer::timeout, this, &DeviceManager::refreshPorts);
    m_pollTimer->start(2000);
    refreshPorts();
}

DeviceManager::~DeviceManager()
{
    disconnectDevice();
}

QStringList DeviceManager::availablePorts() const
{
    QStringList ports;
    const auto infos = QSerialPortInfo::availablePorts();
    for (const auto& info : infos) {
        ports << info.portName();
    }
    return ports;
}

QStringList DeviceManager::availableRtlSdrDevices() const
{
#ifdef RFLOGGER_HAS_RTLSDR
    return RtlSdrDevice::availableDevices();
#else
    return {};
#endif
}

void DeviceManager::refreshPorts()
{
    QStringList ports = availablePorts();

    // Also add RTL-SDR devices
    const auto rtlDevices = availableRtlSdrDevices();
    for (const auto& dev : rtlDevices)
        ports << dev;

    if (ports != m_lastPorts) {
        m_lastPorts = ports;
        emit portsChanged(ports);
    }
}

bool DeviceManager::connectDevice(const QString& portOrPath, DeviceType type)
{
    disconnectDevice();

    if (type == DeviceType::AutoDetect)
        type = detectDeviceType(portOrPath);

    ISpectrumDevice* device = nullptr;

    switch (type) {
    case DeviceType::RFExplorer:
        device = new RFExplorerDevice(this);
        break;
    case DeviceType::TinySA:
        device = new TinySADevice(this);
        break;
    case DeviceType::RtlSdr:
#ifdef RFLOGGER_HAS_RTLSDR
        device = new RtlSdrDevice(this);
#else
        emit errorOccurred(tr("RTL-SDR support not compiled in"));
        return false;
#endif
        break;
    default:
        emit errorOccurred(tr("Could not auto-detect device type on %1").arg(portOrPath));
        return false;
    }

    if (!device->connectDevice(portOrPath)) {
        delete device;
        return false;
    }

    m_currentDevice = device;

    // Forward device signals
    connect(device, &ISpectrumDevice::connectionChanged, this, [this](bool connected) {
        if (!connected) {
            m_currentDevice = nullptr;
            emit deviceDisconnected();
        }
    });
    connect(device, &ISpectrumDevice::errorOccurred, this, &DeviceManager::errorOccurred);

    emit deviceConnected(device);
    return true;
}

void DeviceManager::disconnectDevice()
{
    if (m_currentDevice) {
        m_currentDevice->disconnectDevice();
        m_currentDevice->deleteLater();
        m_currentDevice = nullptr;
        emit deviceDisconnected();
    }
}

DeviceManager::DeviceType DeviceManager::detectDeviceType(const QString& portName) const
{
    // Check if it's an RTL-SDR path
    if (portName.startsWith("rtlsdr:"))
        return DeviceType::RtlSdr;

    // Check serial port info for clues
    const auto infos = QSerialPortInfo::availablePorts();
    for (const auto& info : infos) {
        if (info.portName() == portName) {
            const QString desc = info.description().toLower();
            const quint16 vid = info.vendorIdentifier();
            const quint16 pid = info.productIdentifier();

            // Silicon Labs CP210x — used by both RF Explorer and TinySA
            // RF Explorer VID:PID = 10C4:EA60
            if (vid == 0x10C4 && pid == 0xEA60) {
                // Both devices use the same chip. Try RF Explorer first (most common for this tool).
                return DeviceType::RFExplorer;
            }

            if (desc.contains("rf explorer"))
                return DeviceType::RFExplorer;
            if (desc.contains("tinysa"))
                return DeviceType::TinySA;

            // Default for Silicon Labs devices — try RF Explorer
            if (desc.contains("cp210") || desc.contains("silicon labs"))
                return DeviceType::RFExplorer;

            break;
        }
    }

    return DeviceType::AutoDetect; // Could not determine
}
