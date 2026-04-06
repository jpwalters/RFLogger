#pragma once

#include "data/SweepData.h"

#include <QMainWindow>

class DeviceManager;
class ScanSession;
class SpectrumWidget;
class WaterfallWidget;
class DevicePanel;
class CaptureControls;
class MarkerPanel;
class ISpectrumDevice;
class UpdateChecker;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onDisconnect();
    void onStartScan(double startHz, double stopHz, int points);
    void onStopScan();
    void onSweepReady(const SweepData& sweep);
    void onExport();
    void onAbout();
    void onCheckForUpdates();

private:
    void createMenus();
    void createDockWidgets();
    void saveSettings();
    void loadSettings();
    void updateDeviceLimits(ISpectrumDevice* device);

    DeviceManager* m_deviceManager;
    ScanSession* m_session;

    SpectrumWidget* m_spectrumWidget;
    WaterfallWidget* m_waterfallWidget;
    DevicePanel* m_devicePanel;
    CaptureControls* m_captureControls;
    MarkerPanel* m_markerPanel;
    UpdateChecker* m_updateChecker = nullptr;
};
