#pragma once

#include "devices/DeviceManager.h"

#include <QWidget>
#include <QLabel>
#include <QPushButton>

class DevicePanel : public QWidget
{
    Q_OBJECT

public:
    explicit DevicePanel(DeviceManager* manager, QWidget* parent = nullptr);

signals:
    void disconnectRequested();

private slots:
    void onDeviceConnected(ISpectrumDevice* device);
    void onDeviceDisconnected();
    void onStatusChanged(const QString& status);

private:
    DeviceManager* m_manager;

    QLabel* m_statusLabel;
    QLabel* m_modelLabel;
    QLabel* m_firmwareLabel;
    QLabel* m_freqRangeLabel;
    QLabel* m_portLabel;
    QPushButton* m_disconnectBtn;
    QWidget* m_infoGroup;
};
