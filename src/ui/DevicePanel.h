#pragma once

#include "devices/DeviceManager.h"

#include <QWidget>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>

class DevicePanel : public QWidget
{
    Q_OBJECT

public:
    explicit DevicePanel(DeviceManager* manager, QWidget* parent = nullptr);

    QString selectedPort() const;
    DeviceManager::DeviceType selectedDeviceType() const;

public slots:
    void refreshPorts();

signals:
    void connectRequested(const QString& port, DeviceManager::DeviceType type);
    void disconnectRequested();

private slots:
    void onConnectClicked();
    void onDeviceConnected(ISpectrumDevice* device);
    void onDeviceDisconnected();

private:
    DeviceManager* m_manager;

    QComboBox* m_portCombo;
    QComboBox* m_typeCombo;
    QPushButton* m_connectBtn;
    QLabel* m_statusLabel;
    QLabel* m_modelLabel;
    QLabel* m_firmwareLabel;
    QLabel* m_freqRangeLabel;
};
