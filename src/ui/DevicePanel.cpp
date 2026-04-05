#include "DevicePanel.h"

#include <QVBoxLayout>
#include <QFormLayout>
#include <QGroupBox>

DevicePanel::DevicePanel(DeviceManager* manager, QWidget* parent)
    : QWidget(parent)
    , m_manager(manager)
{
    auto* layout = new QVBoxLayout(this);

    // Connection group
    auto* connGroup = new QGroupBox(tr("Connection"));
    auto* connLayout = new QFormLayout(connGroup);

    m_portCombo = new QComboBox;
    m_portCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connLayout->addRow(tr("Port:"), m_portCombo);

    m_typeCombo = new QComboBox;
    m_typeCombo->addItem(tr("Auto Detect"), static_cast<int>(DeviceManager::DeviceType::AutoDetect));
    m_typeCombo->addItem(tr("RF Explorer"), static_cast<int>(DeviceManager::DeviceType::RFExplorer));
    m_typeCombo->addItem(tr("TinySA"), static_cast<int>(DeviceManager::DeviceType::TinySA));
    m_typeCombo->addItem(tr("RTL-SDR"), static_cast<int>(DeviceManager::DeviceType::RtlSdr));
    connLayout->addRow(tr("Type:"), m_typeCombo);

    m_connectBtn = new QPushButton(tr("Connect"));
    connLayout->addRow(m_connectBtn);

    m_statusLabel = new QLabel(tr("Disconnected"));
    m_statusLabel->setStyleSheet("color: #ff6666; font-weight: bold;");
    connLayout->addRow(tr("Status:"), m_statusLabel);

    layout->addWidget(connGroup);

    // Device info group
    auto* infoGroup = new QGroupBox(tr("Device Info"));
    auto* infoLayout = new QFormLayout(infoGroup);

    m_modelLabel = new QLabel(tr("—"));
    infoLayout->addRow(tr("Model:"), m_modelLabel);

    m_firmwareLabel = new QLabel(tr("—"));
    infoLayout->addRow(tr("Firmware:"), m_firmwareLabel);

    m_freqRangeLabel = new QLabel(tr("—"));
    infoLayout->addRow(tr("Freq Range:"), m_freqRangeLabel);

    layout->addWidget(infoGroup);
    layout->addStretch();

    // Connections
    connect(m_connectBtn, &QPushButton::clicked, this, &DevicePanel::onConnectClicked);
    connect(m_manager, &DeviceManager::portsChanged, this, [this](const QStringList& ports) {
        QString current = m_portCombo->currentText();
        m_portCombo->clear();
        m_portCombo->addItems(ports);
        int idx = m_portCombo->findText(current);
        if (idx >= 0) m_portCombo->setCurrentIndex(idx);
    });
    connect(m_manager, &DeviceManager::deviceConnected, this, &DevicePanel::onDeviceConnected);
    connect(m_manager, &DeviceManager::deviceDisconnected, this, &DevicePanel::onDeviceDisconnected);

    refreshPorts();
}

QString DevicePanel::selectedPort() const
{
    return m_portCombo->currentText();
}

DeviceManager::DeviceType DevicePanel::selectedDeviceType() const
{
    return static_cast<DeviceManager::DeviceType>(m_typeCombo->currentData().toInt());
}

void DevicePanel::refreshPorts()
{
    QStringList ports = m_manager->availablePorts();
    const auto rtl = m_manager->availableRtlSdrDevices();
    ports.append(rtl);

    QString current = m_portCombo->currentText();
    m_portCombo->clear();
    m_portCombo->addItems(ports);
    int idx = m_portCombo->findText(current);
    if (idx >= 0) m_portCombo->setCurrentIndex(idx);
}

void DevicePanel::onConnectClicked()
{
    if (m_manager->isConnected()) {
        emit disconnectRequested();
    } else {
        emit connectRequested(selectedPort(), selectedDeviceType());
    }
}

void DevicePanel::onDeviceConnected(ISpectrumDevice* device)
{
    m_connectBtn->setText(tr("Disconnect"));
    m_statusLabel->setText(tr("Connected"));
    m_statusLabel->setStyleSheet("color: #66ff66; font-weight: bold;");

    m_portCombo->setEnabled(false);
    m_typeCombo->setEnabled(false);

    auto updateInfo = [this, device]() {
        m_modelLabel->setText(device->modelName());
        m_firmwareLabel->setText(device->firmwareVersion().isEmpty()
                                    ? tr("—") : device->firmwareVersion());
        m_freqRangeLabel->setText(
            QString("%1 – %2 MHz")
                .arg(device->minFreqHz() / 1e6, 0, 'f', 1)
                .arg(device->maxFreqHz() / 1e6, 0, 'f', 1));
    };

    connect(device, &ISpectrumDevice::deviceInfoUpdated, this, updateInfo);
    updateInfo();
}

void DevicePanel::onDeviceDisconnected()
{
    m_connectBtn->setText(tr("Connect"));
    m_statusLabel->setText(tr("Disconnected"));
    m_statusLabel->setStyleSheet("color: #ff6666; font-weight: bold;");

    m_portCombo->setEnabled(true);
    m_typeCombo->setEnabled(true);

    m_modelLabel->setText(tr("—"));
    m_firmwareLabel->setText(tr("—"));
    m_freqRangeLabel->setText(tr("—"));
}
