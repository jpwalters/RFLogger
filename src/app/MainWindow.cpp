#include "MainWindow.h"
#include "AboutDialog.h"
#include "UpdateChecker.h"
#include "SettingsManager.h"
#include "devices/DeviceManager.h"
#include "devices/ISpectrumDevice.h"
#include "data/ScanSession.h"
#include "ui/SpectrumWidget.h"
#include "ui/WaterfallWidget.h"
#include "ui/DevicePanel.h"
#include "ui/CaptureControls.h"
#include "ui/MarkerPanel.h"
#include "ui/ExportDialog.h"

#include <QSplitter>
#include <QDockWidget>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QStatusBar>
#include <QLabel>
#include <QCloseEvent>
#include <QMessageBox>
#include <QApplication>
#include <stdexcept>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_deviceManager(new DeviceManager(this))
    , m_session(new ScanSession(this))
{
    setWindowTitle(tr("RF Logger — RF Spectrum Analyzer"));
    setMinimumSize(1024, 600);

    // Central widget: splitter with spectrum on top, waterfall on bottom
    auto* splitter = new QSplitter(Qt::Vertical);

    m_spectrumWidget = new SpectrumWidget;
    m_waterfallWidget = new WaterfallWidget;

    splitter->addWidget(m_spectrumWidget);
    splitter->addWidget(m_waterfallWidget);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 1);
    setCentralWidget(splitter);

    createDockWidgets();
    createMenus();

    // Status bar
    statusBar()->showMessage(tr("Waiting for device..."));

    // Wire up connections
    connect(m_devicePanel, &DevicePanel::disconnectRequested, this, &MainWindow::onDisconnect);

    connect(m_captureControls, &CaptureControls::startScanRequested, this, &MainWindow::onStartScan);
    connect(m_captureControls, &CaptureControls::stopScanRequested, this, &MainWindow::onStopScan);
    connect(m_captureControls, &CaptureControls::showLiveChanged, m_spectrumWidget, &SpectrumWidget::setShowLive);
    connect(m_captureControls, &CaptureControls::showMaxHoldChanged, m_spectrumWidget, &SpectrumWidget::setShowMaxHold);
    connect(m_captureControls, &CaptureControls::showAverageChanged, m_spectrumWidget, &SpectrumWidget::setShowAverage);

    connect(m_markerPanel, &MarkerPanel::markersChanged, m_spectrumWidget, &SpectrumWidget::setMarkers);
    connect(m_spectrumWidget, &SpectrumWidget::frequencyClicked, m_markerPanel, &MarkerPanel::addMarkerAtFrequency);

    connect(m_deviceManager, &DeviceManager::deviceConnected, this, [this](ISpectrumDevice* device) {
        updateDeviceLimits(device);
        statusBar()->showMessage(tr("Connected to %1").arg(device->deviceName()));
    });
    connect(m_deviceManager, &DeviceManager::deviceDisconnected, this, [this]() {
        statusBar()->showMessage(tr("Device disconnected — waiting for device..."));
    });
    connect(m_deviceManager, &DeviceManager::errorOccurred, this, [this](const QString& msg) {
        statusBar()->showMessage(tr("Error: %1").arg(msg));
    });
    connect(m_deviceManager, &DeviceManager::statusChanged, this, [this](const QString& status) {
        statusBar()->showMessage(status);
    });

    loadSettings();
}

MainWindow::~MainWindow()
{
}

void MainWindow::createMenus()
{
    // File menu
    auto* fileMenu = menuBar()->addMenu(tr("&File"));

    auto* exportAction = fileMenu->addAction(tr("&Export Scan..."), this, &MainWindow::onExport, QKeySequence("Ctrl+E"));
    exportAction->setEnabled(true);

    fileMenu->addSeparator();
    fileMenu->addAction(tr("E&xit"), this, &QWidget::close, QKeySequence::Quit);

    // View menu
    auto* viewMenu = menuBar()->addMenu(tr("&View"));
    viewMenu->addAction(tr("&Reset Zoom"), m_spectrumWidget, [this]() {
        auto* dev = m_deviceManager->currentDevice();
        if (dev) {
            m_spectrumWidget->setFrequencyRange(
                m_captureControls->startFreqMHz(),
                m_captureControls->stopFreqMHz());
        }
        m_spectrumWidget->setAmplitudeRange(-120, 0);
    }, QKeySequence("Ctrl+0"));

    viewMenu->addAction(tr("&Clear Traces"), this, [this]() {
        m_spectrumWidget->clearAll();
        m_waterfallWidget->clear();
        m_session->clear();
    }, QKeySequence("Ctrl+L"));

    // Help menu
    auto* helpMenu = menuBar()->addMenu(tr("&Help"));
    helpMenu->addAction(tr("Check for &Updates..."), this, &MainWindow::onCheckForUpdates);
    helpMenu->addSeparator();
    helpMenu->addAction(tr("&About RF Logger"), this, &MainWindow::onAbout);
    helpMenu->addAction(tr("About &Qt"), qApp, &QApplication::aboutQt);

#ifdef RFLOGGER_DEBUG_MENU
    // Debug menu — test crash handlers
    auto* debugMenu = menuBar()->addMenu(tr("&Debug"));
    debugMenu->addAction(tr("Test: C++ Exception"), this, []() {
        throw std::runtime_error("Test crash — this is a deliberate C++ exception to verify the crash handler.");
    });
    debugMenu->addAction(tr("Test: Access Violation"), this, []() {
        volatile int* p = nullptr;
        *p = 42;  // NOLINT — deliberate null dereference
    });
    debugMenu->addAction(tr("Test: Abort"), this, []() {
        std::abort();
    });
    debugMenu->addAction(tr("Test: Qt Fatal"), this, []() {
        qFatal("Test crash — this is a deliberate qFatal() to verify the crash handler.");
    });
#endif
}

void MainWindow::createDockWidgets()
{
    // Left dock: Device panel
    auto* deviceDock = new QDockWidget(tr("Device"), this);
    m_devicePanel = new DevicePanel(m_deviceManager);
    deviceDock->setWidget(m_devicePanel);
    deviceDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    addDockWidget(Qt::LeftDockWidgetArea, deviceDock);

    // Left dock: Capture controls (below device)
    auto* captureDock = new QDockWidget(tr("Capture"), this);
    m_captureControls = new CaptureControls;
    captureDock->setWidget(m_captureControls);
    captureDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    addDockWidget(Qt::LeftDockWidgetArea, captureDock);

    // Stack device and capture docks vertically
    splitDockWidget(deviceDock, captureDock, Qt::Vertical);

    // Right dock: Markers
    auto* markerDock = new QDockWidget(tr("Markers"), this);
    m_markerPanel = new MarkerPanel;
    markerDock->setWidget(m_markerPanel);
    markerDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    addDockWidget(Qt::RightDockWidgetArea, markerDock);
}

void MainWindow::onDisconnect()
{
    onStopScan();
    m_deviceManager->disconnectDevice();
}

void MainWindow::onStartScan(double startHz, double stopHz, int points)
{
    auto* device = m_deviceManager->currentDevice();
    if (!device) {
        statusBar()->showMessage(tr("No device connected"));
        return;
    }

    // Clear previous session
    m_session->clear();
    m_spectrumWidget->clearAll();
    m_waterfallWidget->clear();

    // Set frequency range on spectrum widget
    m_spectrumWidget->setFrequencyRange(startHz / 1e6, stopHz / 1e6);

    // Configure device
    if (!device->configure(startHz, stopHz, points)) {
        statusBar()->showMessage(tr("Failed to configure device"));
        return;
    }

    // Connect sweep signal
    connect(device, &ISpectrumDevice::sweepReady, this, &MainWindow::onSweepReady,
            Qt::UniqueConnection);

    // Start
    m_session->setDeviceName(device->deviceName());
    m_session->setStartTime(QDateTime::currentDateTimeUtc());

    device->startScanning();
    m_captureControls->onScanStarted();
    statusBar()->showMessage(tr("Scanning %1 – %2 MHz (%3 points)")
                                 .arg(startHz / 1e6, 0, 'f', 3)
                                 .arg(stopHz / 1e6, 0, 'f', 3)
                                 .arg(points));
}

void MainWindow::onStopScan()
{
    auto* device = m_deviceManager->currentDevice();
    if (device && device->isScanning()) {
        device->stopScanning();
        m_session->setStopTime(QDateTime::currentDateTimeUtc());
    }
    m_captureControls->onScanStopped();
    statusBar()->showMessage(tr("Scan stopped — %1 sweeps captured").arg(m_session->sweepCount()));
}

void MainWindow::onSweepReady(const SweepData& sweep)
{
    m_session->addSweep(sweep);

    if (m_captureControls->showLive())
        m_spectrumWidget->updateLive(sweep);
    if (m_captureControls->showMaxHold())
        m_spectrumWidget->updateMaxHold(m_session->maxHold());
    if (m_captureControls->showAverage())
        m_spectrumWidget->updateAverage(m_session->average());

    m_waterfallWidget->addSweep(sweep);
    m_captureControls->onSweepReceived();
}

void MainWindow::onExport()
{
    if (m_session->isEmpty()) {
        QMessageBox::information(this, tr("Export"), tr("No scan data to export. Run a scan first."));
        return;
    }

    ExportDialog dlg(m_session, this);
    dlg.exec();
}

void MainWindow::onAbout()
{
    AboutDialog dlg(this);
    dlg.exec();
}

void MainWindow::onCheckForUpdates()
{
    if (!m_updateChecker) {
        m_updateChecker = new UpdateChecker(this);

        connect(m_updateChecker, &UpdateChecker::updateAvailable, this,
                [this](const QString& version, const QString& url) {
                    QMessageBox msgBox(this);
                    msgBox.setIcon(QMessageBox::Information);
                    msgBox.setWindowTitle(tr("Update Available"));
                    msgBox.setText(tr("A new version of RF Logger is available: <b>%1</b>").arg(version));
                    msgBox.setInformativeText(
                        tr("<a href=\"%1\">Download from GitHub</a>").arg(url));
                    msgBox.setStandardButtons(QMessageBox::Ok);
                    msgBox.exec();
                });

        connect(m_updateChecker, &UpdateChecker::upToDate, this, [this]() {
            QMessageBox::information(this, tr("No Updates"),
                                    tr("You are running the latest version of RF Logger."));
        });

        connect(m_updateChecker, &UpdateChecker::checkFailed, this,
                [this](const QString& error) {
                    QMessageBox::warning(this, tr("Update Check Failed"),
                                         tr("Could not check for updates:\n%1").arg(error));
                });
    }

    statusBar()->showMessage(tr("Checking for updates..."));
    m_updateChecker->checkForUpdates();
}

void MainWindow::updateDeviceLimits(ISpectrumDevice* device)
{
    connect(device, &ISpectrumDevice::deviceInfoUpdated, this, [this, device]() {
        double minMHz = device->minFreqHz() / 1e6;
        double maxMHz = device->maxFreqHz() / 1e6;

        if (minMHz > 0 && maxMHz > 0) {
            m_captureControls->setFrequencyLimits(minMHz, maxMHz);
            m_captureControls->setSweepPointRange(device->minSweepPoints(), device->maxSweepPoints());
        }
    }, Qt::UniqueConnection);
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    auto* device = m_deviceManager->currentDevice();
    if (device && device->isScanning())
        onStopScan();

    saveSettings();
    event->accept();
}

void MainWindow::saveSettings()
{
    SettingsManager::saveWindowGeometry(this);
    SettingsManager::setValue("capture/startFreqMHz", m_captureControls->startFreqMHz());
    SettingsManager::setValue("capture/stopFreqMHz", m_captureControls->stopFreqMHz());
    SettingsManager::setValue("capture/sweepPoints", m_captureControls->sweepPoints());
}

void MainWindow::loadSettings()
{
    SettingsManager::loadWindowGeometry(this);

    double startMHz = SettingsManager::value("capture/startFreqMHz", 470.0).toDouble();
    double stopMHz = SettingsManager::value("capture/stopFreqMHz", 700.0).toDouble();
    int points = SettingsManager::value("capture/sweepPoints", 112).toInt();

    m_captureControls->setFrequencyRange(startMHz, stopMHz);
    m_spectrumWidget->setFrequencyRange(startMHz, stopMHz);
}
