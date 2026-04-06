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
    , m_autoConnectTimer(new QTimer(this))
{
    // Poll for USB hotplug every 2 seconds
    connect(m_pollTimer, &QTimer::timeout, this, &DeviceManager::refreshPorts);
    m_pollTimer->start(2000);

    // Auto-connect timer fires once after a short delay when new ports appear
    m_autoConnectTimer->setSingleShot(true);
    connect(m_autoConnectTimer, &QTimer::timeout, this, &DeviceManager::tryAutoConnect);

    emit statusChanged(tr("Waiting for device..."));
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

    // Check if connected port disappeared (USB unplug fallback)
    if (isConnected() && !m_connectedPort.isEmpty() && !ports.contains(m_connectedPort)) {
        qDebug() << "Connected port" << m_connectedPort << "disappeared — disconnecting";
        disconnectDevice();
        emit statusChanged(tr("Device disconnected — waiting for device..."));
    }

    if (ports != m_lastPorts) {
        // Find newly appeared ports
        QStringList newPorts;
        for (const auto& p : ports) {
            if (!m_lastPorts.contains(p))
                newPorts << p;
        }

        m_lastPorts = ports;
        emit portsChanged(ports);

        // If not connected and new ports appeared, schedule auto-connect
        if (!isConnected() && !newPorts.isEmpty()) {
            m_pendingPorts = newPorts;
            m_autoConnectTimer->start(500); // brief delay for port initialization
        }
    }

    // If not connected and no auto-connect pending, keep showing waiting status
    if (!isConnected() && !m_autoConnectTimer->isActive() && m_lastPorts.isEmpty()) {
        emit statusChanged(tr("Waiting for device..."));
    }
}

void DeviceManager::tryAutoConnect()
{
    if (isConnected())
        return;

    // Try each pending new port
    for (const auto& port : std::as_const(m_pendingPorts)) {
        emit autoConnectAttempting(port);
        emit statusChanged(tr("Connecting to %1...").arg(port));

        if (connectDevice(port, DeviceType::AutoDetect)) {
            m_pendingPorts.clear();
            return;
        }
    }

    m_pendingPorts.clear();

    // If all ports were already known but we're not connected, try them all
    if (!isConnected() && !m_lastPorts.isEmpty()) {
        for (const auto& port : std::as_const(m_lastPorts)) {
            emit autoConnectAttempting(port);
            emit statusChanged(tr("Connecting to %1...").arg(port));

            if (connectDevice(port, DeviceType::AutoDetect))
                return;
        }
    }

    emit statusChanged(tr("Waiting for device..."));
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
    m_connectedPort = portOrPath;

    // Forward device signals and handle unexpected disconnects
    connect(device, &ISpectrumDevice::connectionChanged, this, [this](bool connected) {
        if (!connected) {
            // The device object will be cleaned up by whoever owns it —
            // just null our pointer to prevent double-free
            m_currentDevice = nullptr;
            m_connectedPort.clear();
            emit deviceDisconnected();
            emit statusChanged(tr("Device disconnected — waiting for device..."));
        }
    });
    connect(device, &ISpectrumDevice::errorOccurred, this, &DeviceManager::errorOccurred);

    emit statusChanged(tr("Connected to %1").arg(device->deviceName()));
    emit deviceConnected(device);
    return true;
}

void DeviceManager::disconnectDevice()
{
    if (m_currentDevice) {
        m_currentDevice->disconnectDevice();
        m_currentDevice->deleteLater();
        m_currentDevice = nullptr;
        m_connectedPort.clear();
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
