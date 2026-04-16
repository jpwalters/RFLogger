#include "DevicePanel.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
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

    // Gain control group (hidden until a gain-capable device connects)
    m_gainGroup = new QGroupBox(tr("Gain"));
    auto* gainLayout = new QVBoxLayout(m_gainGroup);

    m_gainSlider = new QSlider(Qt::Horizontal);
    m_gainSlider->setMinimum(0);
    m_gainSlider->setMaximum(0);
    m_gainSlider->setTickPosition(QSlider::TicksBelow);
    m_gainSlider->setTickInterval(1);
    gainLayout->addWidget(m_gainSlider);

    m_gainLabel = new QLabel(tr("0.0 dB"));
    m_gainLabel->setAlignment(Qt::AlignCenter);
    gainLayout->addWidget(m_gainLabel);

    m_gainGroup->setVisible(false);
    layout->addWidget(m_gainGroup);

    // Disconnect button (hidden until connected)
    m_disconnectBtn = new QPushButton(tr("Disconnect"));
    m_disconnectBtn->setVisible(false);
    layout->addWidget(m_disconnectBtn);

    layout->addStretch();

    // Connections
    connect(m_disconnectBtn, &QPushButton::clicked, this, &DevicePanel::disconnectRequested);
    connect(m_gainSlider, &QSlider::valueChanged, this, &DevicePanel::onGainSliderChanged);
    connect(m_manager, &DeviceManager::deviceConnected, this, &DevicePanel::onDeviceConnected);
    connect(m_manager, &DeviceManager::deviceDisconnected, this, &DevicePanel::onDeviceDisconnected);
    connect(m_manager, &DeviceManager::statusChanged, this, &DevicePanel::onStatusChanged);
}

void DevicePanel::onDeviceConnected(ISpectrumDevice* device)
{
    m_currentDevice = device;

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

    // Gain control — show only for devices that support it
    if (device->supportsGainControl()) {
        m_availableGains = device->availableGains();
        if (!m_availableGains.isEmpty()) {
            m_gainSlider->setMaximum(m_availableGains.size() - 1);

            // Set slider to current gain position
            int currentGain = device->currentGain();
            int pos = 0;
            for (int i = 0; i < m_availableGains.size(); ++i) {
                if (m_availableGains[i] == currentGain) {
                    pos = i;
                    break;
                }
            }
            m_gainSlider->setValue(pos);
            m_gainLabel->setText(QStringLiteral("%1 dB").arg(currentGain / 10.0, 0, 'f', 1));
            m_gainGroup->setVisible(true);

            connect(device, &ISpectrumDevice::gainChanged,
                    this, &DevicePanel::onDeviceGainChanged);
        }
    }
}

void DevicePanel::onDeviceDisconnected()
{
    m_currentDevice = nullptr;

    m_statusLabel->setText(tr("Waiting for device..."));
    m_statusLabel->setStyleSheet("font-weight: bold; color: #ff9900; padding: 4px;");

    m_infoGroup->setVisible(false);
    m_disconnectBtn->setVisible(false);
    m_gainGroup->setVisible(false);
    m_availableGains.clear();

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

void DevicePanel::onGainSliderChanged(int position)
{
    if (!m_currentDevice || position < 0 || position >= m_availableGains.size())
        return;

    int tenthsDb = m_availableGains[position];
    m_gainLabel->setText(QStringLiteral("%1 dB").arg(tenthsDb / 10.0, 0, 'f', 1));
    m_currentDevice->setGain(tenthsDb);
}

void DevicePanel::onDeviceGainChanged(int tenthsDb)
{
    // Update slider position to match (e.g. if gain was set programmatically)
    for (int i = 0; i < m_availableGains.size(); ++i) {
        if (m_availableGains[i] == tenthsDb) {
            m_gainSlider->blockSignals(true);
            m_gainSlider->setValue(i);
            m_gainSlider->blockSignals(false);
            break;
        }
    }
    m_gainLabel->setText(QStringLiteral("%1 dB").arg(tenthsDb / 10.0, 0, 'f', 1));
}
