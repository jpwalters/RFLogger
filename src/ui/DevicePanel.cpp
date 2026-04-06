#include "DevicePanel.h"

#include <QVBoxLayout>
#include <QFormLayout>
#include <QGroupBox>

DevicePanel::DevicePanel(DeviceManager* manager, QWidget* parent)
    : QWidget(parent)
    , m_manager(manager)
{
    auto* layout = new QVBoxLayout(this);

    // Status section
    m_statusLabel = new QLabel(tr("Waiting for device..."));
    m_statusLabel->setStyleSheet("font-weight: bold; color: #ff9900; padding: 4px;");
    m_statusLabel->setWordWrap(true);
    layout->addWidget(m_statusLabel);

    // Device info group (hidden until connected)
    m_infoGroup = new QGroupBox(tr("Device Info"));
    auto* infoLayout = new QFormLayout(static_cast<QGroupBox*>(m_infoGroup));

    m_modelLabel = new QLabel(tr("—"));
    infoLayout->addRow(tr("Model:"), m_modelLabel);

    m_firmwareLabel = new QLabel(tr("—"));
    infoLayout->addRow(tr("Firmware:"), m_firmwareLabel);

    m_freqRangeLabel = new QLabel(tr("—"));
    infoLayout->addRow(tr("Freq Range:"), m_freqRangeLabel);

    m_portLabel = new QLabel(tr("—"));
    infoLayout->addRow(tr("Port:"), m_portLabel);

    m_infoGroup->setVisible(false);
    layout->addWidget(m_infoGroup);

    // Disconnect button (hidden until connected)
    m_disconnectBtn = new QPushButton(tr("Disconnect"));
    m_disconnectBtn->setVisible(false);
    layout->addWidget(m_disconnectBtn);

    layout->addStretch();

    // Connections
    connect(m_disconnectBtn, &QPushButton::clicked, this, &DevicePanel::disconnectRequested);
    connect(m_manager, &DeviceManager::deviceConnected, this, &DevicePanel::onDeviceConnected);
    connect(m_manager, &DeviceManager::deviceDisconnected, this, &DevicePanel::onDeviceDisconnected);
    connect(m_manager, &DeviceManager::statusChanged, this, &DevicePanel::onStatusChanged);
}

void DevicePanel::onDeviceConnected(ISpectrumDevice* device)
{
    m_statusLabel->setText(tr("Connected"));
    m_statusLabel->setStyleSheet("font-weight: bold; color: #66ff66; padding: 4px;");

    m_infoGroup->setVisible(true);
    m_disconnectBtn->setVisible(true);
    m_portLabel->setText(m_manager->connectedPort());

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
    m_statusLabel->setText(tr("Waiting for device..."));
    m_statusLabel->setStyleSheet("font-weight: bold; color: #ff9900; padding: 4px;");

    m_infoGroup->setVisible(false);
    m_disconnectBtn->setVisible(false);

    m_modelLabel->setText(tr("—"));
    m_firmwareLabel->setText(tr("—"));
    m_freqRangeLabel->setText(tr("—"));
    m_portLabel->setText(tr("—"));
}

void DevicePanel::onStatusChanged(const QString& status)
{
    if (!m_manager->isConnected()) {
        m_statusLabel->setText(status);
        if (status.contains("Connecting"))
            m_statusLabel->setStyleSheet("font-weight: bold; color: #66ccff; padding: 4px;");
        else
            m_statusLabel->setStyleSheet("font-weight: bold; color: #ff9900; padding: 4px;");
    }
}
