#pragma once

#include "data/SweepData.h"

#include <QMainWindow>
#include <QMap>

class QTimer;
class QLabel;
class QActionGroup;
class QMenu;
class DeviceManager;
class ScanSession;
class SpectrumWidget;
class WaterfallWidget;
class DevicePanel;
class CaptureControls;
class MarkerPanel;
class ExportPanel;
class FrequencyListPanel;
class ISpectrumDevice;
class UpdateChecker;
class InterferenceMonitor;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

private slots:
    void onDisconnect();
    void onStartScan(double startHz, double stopHz, int points);
    void onStopScan();
    void onSweepReady(const SweepData& sweep);
    void onExport();
    void onExportFromPanel(const QString& format, const QString& dataSource, const QString& filePath);
    void onAbout();
    void onCheckForUpdates();
    void onSaveSession();
    void onLoadReferenceTrace();
    void onClearReferenceTrace();
    void onImportCsvTrace();

private:
    void createMenus();
    void createDockWidgets();
    void saveSettings();
    void loadSettings();
    void updateDeviceLimits(ISpectrumDevice* device);
    void toggleFullScreen();
    void updateSignalClassifier();
    void rebuildImportedTracesMenu();

    DeviceManager* m_deviceManager;
    ScanSession* m_session;

    SpectrumWidget* m_spectrumWidget;
    WaterfallWidget* m_waterfallWidget;
    DevicePanel* m_devicePanel;
    CaptureControls* m_captureControls;
    MarkerPanel* m_markerPanel;
    ExportPanel* m_exportPanel;
    FrequencyListPanel* m_frequencyListPanel;
    QDockWidget* m_deviceDock = nullptr;
    QDockWidget* m_captureDock = nullptr;
    QDockWidget* m_exportDock = nullptr;
    QDockWidget* m_markerDock = nullptr;
    QDockWidget* m_frequencyListDock = nullptr;
    UpdateChecker* m_updateChecker = nullptr;
    InterferenceMonitor* m_interferenceMonitor = nullptr;

    // Full-screen mode
    bool m_fullScreen = false;
    QAction* m_fullScreenAction = nullptr;
    QMap<QDockWidget*, bool> m_dockVisibilityBeforeFullScreen;
    QTimer* m_cursorHideTimer = nullptr;
    QLabel* m_fullScreenHint = nullptr;

    // TV channel overlay
    QAction* m_showTvBandsAction = nullptr;
    QMenu* m_tvRegionMenu = nullptr;
    QActionGroup* m_tvRegionGroup = nullptr;

    // Band plan overlay
    QAction* m_showBandPlanAction = nullptr;
    QMenu* m_bandPlanRegionMenu = nullptr;
    QActionGroup* m_bandPlanRegionGroup = nullptr;

    // Reference trace
    QAction* m_showReferenceAction = nullptr;
    QAction* m_clearReferenceAction = nullptr;

    // Imported CSV trace overlays
    QMenu* m_importedTracesMenu = nullptr;

    // Interference monitor
    QAction* m_interferenceAlertAction = nullptr;
    QLabel* m_alertIndicator = nullptr;

    // Quick-start: show a fast low-res first sweep before switching to full resolution
    bool m_quickStartPending = false;
    bool m_amplitudeAutoFitPending = false;
    double m_fullResStartHz = 0.0;
    double m_fullResStopHz = 0.0;
    int m_fullResPoints = 0;
};
