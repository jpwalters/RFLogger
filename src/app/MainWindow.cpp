#include "MainWindow.h"
#include "AboutDialog.h"
#include "UpdateChecker.h"
#include "SettingsManager.h"
#include "devices/DeviceManager.h"
#include "devices/ISpectrumDevice.h"
#include "data/ScanSession.h"
#include "data/TvChannelMap.h"
#include "data/RfBandPlan.h"
#include "data/SessionSerializer.h"
#include "data/SignalClassifier.h"
#include "data/InterferenceMonitor.h"
#include "ui/SpectrumWidget.h"
#include "ui/WaterfallWidget.h"
#include "ui/DevicePanel.h"
#include "ui/CaptureControls.h"
#include "ui/MarkerPanel.h"
#include "ui/ExportDialog.h"
#include "ui/ExportPanel.h"
#include "ui/FrequencyListPanel.h"
#include "export/WwbExporter.h"
#include "export/GenericCsvExporter.h"

#include <QSplitter>
#include <QDockWidget>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QActionGroup>
#include <QStatusBar>
#include <QLabel>
#include <QCloseEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QMessageBox>
#include <QApplication>
#include <QTimer>
#include <QFileDialog>
#include <QFileInfo>
#include <stdexcept>
#include <cmath>

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
    splitter->setStretchFactor(0, 5);
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

    connect(m_markerPanel, &MarkerPanel::markersChanged, m_spectrumWidget, &SpectrumWidget::setMarkers);
    connect(m_spectrumWidget, &SpectrumWidget::frequencyClicked, m_markerPanel, &MarkerPanel::addMarkerAtFrequency);

    // TV channel click → highlight
    connect(m_spectrumWidget, &SpectrumWidget::tvChannelClicked,
            m_spectrumWidget, &SpectrumWidget::highlightTvChannel);

    connect(m_deviceManager, &DeviceManager::deviceConnected, this, [this](ISpectrumDevice* device) {
        updateDeviceLimits(device);
        m_frequencyListPanel->setDemodulationAvailable(device->supportsDemodulation());
        statusBar()->showMessage(tr("Connected to %1").arg(device->deviceName()));
    });
    connect(m_deviceManager, &DeviceManager::deviceDisconnected, this, [this]() {
        m_frequencyListPanel->setDemodulationAvailable(false);
        statusBar()->showMessage(tr("Device disconnected — waiting for device..."));
    });
    connect(m_deviceManager, &DeviceManager::errorOccurred, this, [this](const QString& msg) {
        statusBar()->showMessage(tr("Error: %1").arg(msg));
    });
    connect(m_deviceManager, &DeviceManager::statusChanged, this, [this](const QString& status) {
        statusBar()->showMessage(status);
    });

    // Full-screen: cursor-hide timer and hint overlay
    m_cursorHideTimer = new QTimer(this);
    m_cursorHideTimer->setSingleShot(true);
    m_cursorHideTimer->setInterval(3000);
    connect(m_cursorHideTimer, &QTimer::timeout, this, [this]() {
        if (m_fullScreen) {
            qApp->setOverrideCursor(Qt::BlankCursor);
            m_fullScreenHint->hide();
            m_spectrumWidget->setCrosshairVisible(false);
        }
    });

    m_fullScreenHint = new QLabel(tr("Press ESC to exit full screen"), this);
    m_fullScreenHint->setStyleSheet(
        "background-color: rgba(0, 0, 0, 160);"
        "color: rgba(255, 255, 255, 200);"
        "padding: 8px 16px;"
        "border-radius: 4px;"
        "font-size: 13px;"
    );
    m_fullScreenHint->setAlignment(Qt::AlignCenter);
    m_fullScreenHint->adjustSize();
    m_fullScreenHint->hide();

    qApp->installEventFilter(this);

    // Interference monitor
    m_interferenceMonitor = new InterferenceMonitor(this);
    connect(m_interferenceMonitor, &InterferenceMonitor::alertTriggered, this,
            [this](const InterferenceAlert& alert) {
                // Flash the alert indicator
                m_alertIndicator->setText(QString("⚠ ALERT: %1").arg(alert.description));
                m_alertIndicator->setVisible(true);
                // Auto-hide after 5 seconds
                QTimer::singleShot(5000, m_alertIndicator, [this]() {
                    m_alertIndicator->setVisible(false);
                });
                statusBar()->showMessage(alert.description, 5000);
            });

    // Alert indicator in status bar
    m_alertIndicator = new QLabel(this);
    m_alertIndicator->setStyleSheet(
        "QLabel { color: #ff4444; font-weight: bold; padding: 2px 8px; }");
    m_alertIndicator->setVisible(false);
    statusBar()->addPermanentWidget(m_alertIndicator);

    loadSettings();
}

MainWindow::~MainWindow()
{
}

void MainWindow::createMenus()
{
    // File menu
    auto* fileMenu = menuBar()->addMenu(tr("&File"));

    fileMenu->addAction(tr("&Save Session..."), QKeySequence("Ctrl+S"), this, &MainWindow::onSaveSession);
    fileMenu->addSeparator();

    fileMenu->addAction(tr("Load &Reference Trace..."), QKeySequence("Ctrl+R"), this, &MainWindow::onLoadReferenceTrace);
    m_clearReferenceAction = fileMenu->addAction(tr("Clear Reference Trace"), this, &MainWindow::onClearReferenceTrace);
    m_clearReferenceAction->setEnabled(false);
    fileMenu->addSeparator();

    fileMenu->addAction(tr("&Import CSV Trace..."), QKeySequence("Ctrl+I"), this, &MainWindow::onImportCsvTrace);
    fileMenu->addSeparator();

    auto* exportAction = fileMenu->addAction(tr("&Export Scan..."), QKeySequence("Ctrl+E"), this, &MainWindow::onExport);
    exportAction->setEnabled(true);

    fileMenu->addSeparator();
    fileMenu->addAction(tr("E&xit"), QKeySequence::Quit, this, &QWidget::close);

    // View menu
    auto* viewMenu = menuBar()->addMenu(tr("&View"));
    viewMenu->addAction(tr("&Reset Zoom"), QKeySequence("Ctrl+0"), m_spectrumWidget, [this]() {
        auto* dev = m_deviceManager->currentDevice();
        if (dev) {
            m_spectrumWidget->setFrequencyRange(
                m_captureControls->startFreqMHz(),
                m_captureControls->stopFreqMHz());
        }
        m_spectrumWidget->setAmplitudeRange(-120, 5);
    });

    viewMenu->addAction(tr("&Clear Traces"), QKeySequence("Ctrl+L"), this, [this]() {
        m_spectrumWidget->clearAll();
        m_waterfallWidget->clear();
        m_session->clear();
    });

    viewMenu->addSeparator();
    viewMenu->addAction(m_markerDock->toggleViewAction());
    viewMenu->addAction(m_frequencyListDock->toggleViewAction());

    viewMenu->addSeparator();
    m_fullScreenAction = viewMenu->addAction(tr("&Full Screen"));
    m_fullScreenAction->setCheckable(true);
    m_fullScreenAction->setShortcut(QKeySequence(Qt::Key_F11));
    m_fullScreenAction->setShortcutContext(Qt::ApplicationShortcut);
    connect(m_fullScreenAction, &QAction::triggered, this, &MainWindow::toggleFullScreen);

    viewMenu->addSeparator();
    auto* showLiveAction = viewMenu->addAction(tr("Show &Live Trace"));
    showLiveAction->setCheckable(true);
    showLiveAction->setChecked(true);
    connect(showLiveAction, &QAction::toggled, this, [this](bool checked) {
        m_captureControls->setShowLive(checked);
        m_spectrumWidget->setShowLive(checked);
    });

    auto* showMaxHoldAction = viewMenu->addAction(tr("Show &Max Hold"));
    showMaxHoldAction->setCheckable(true);
    showMaxHoldAction->setChecked(true);
    connect(showMaxHoldAction, &QAction::toggled, this, [this](bool checked) {
        m_captureControls->setShowMaxHold(checked);
        m_spectrumWidget->setShowMaxHold(checked);
    });

    auto* showAverageAction = viewMenu->addAction(tr("Show &Average"));
    showAverageAction->setCheckable(true);
    showAverageAction->setChecked(true);
    connect(showAverageAction, &QAction::toggled, this, [this](bool checked) {
        m_captureControls->setShowAverage(checked);
        m_spectrumWidget->setShowAverage(checked);
    });

    // TV channel band overlay
    viewMenu->addSeparator();
    m_showTvBandsAction = viewMenu->addAction(tr("Show &TV Channels"));
    m_showTvBandsAction->setCheckable(true);
    m_showTvBandsAction->setChecked(false);
    connect(m_showTvBandsAction, &QAction::toggled, this, [this](bool checked) {
        m_spectrumWidget->setShowTvBands(checked);
        m_tvRegionMenu->setEnabled(checked);
    });

    m_tvRegionMenu = viewMenu->addMenu(tr("TV &Region"));
    m_tvRegionMenu->setEnabled(false);
    m_tvRegionGroup = new QActionGroup(this);
    m_tvRegionGroup->setExclusive(true);
    for (const auto& region : TvChannelMap::availableRegions()) {
        auto* action = m_tvRegionMenu->addAction(region);
        action->setCheckable(true);
        m_tvRegionGroup->addAction(action);
        connect(action, &QAction::triggered, this, [this, region]() {
            m_spectrumWidget->setTvChannelMap(TvChannelMap::forRegion(region));
            updateSignalClassifier();
        });
    }
    // Default to first region
    if (!m_tvRegionGroup->actions().isEmpty())
        m_tvRegionGroup->actions().first()->setChecked(true);

    // Band plan overlay
    viewMenu->addSeparator();
    m_showBandPlanAction = viewMenu->addAction(tr("Show &Band Plan"));
    m_showBandPlanAction->setCheckable(true);
    m_showBandPlanAction->setChecked(false);
    connect(m_showBandPlanAction, &QAction::toggled, this, [this](bool checked) {
        m_spectrumWidget->setShowBandPlan(checked);
        m_bandPlanRegionMenu->setEnabled(checked);
    });

    m_bandPlanRegionMenu = viewMenu->addMenu(tr("Band Plan Re&gion"));
    m_bandPlanRegionMenu->setEnabled(false);
    m_bandPlanRegionGroup = new QActionGroup(this);
    m_bandPlanRegionGroup->setExclusive(true);
    for (const auto& region : RfBandPlan::availableRegions()) {
        auto* action = m_bandPlanRegionMenu->addAction(region);
        action->setCheckable(true);
        m_bandPlanRegionGroup->addAction(action);
        connect(action, &QAction::triggered, this, [this, region]() {
            m_spectrumWidget->setBandPlanRegion(region);
            updateSignalClassifier();
        });
    }
    if (!m_bandPlanRegionGroup->actions().isEmpty())
        m_bandPlanRegionGroup->actions().first()->setChecked(true);

    // Reference trace
    viewMenu->addSeparator();
    m_showReferenceAction = viewMenu->addAction(tr("Show R&eference Trace"));
    m_showReferenceAction->setCheckable(true);
    m_showReferenceAction->setChecked(true);
    m_showReferenceAction->setEnabled(false);
    connect(m_showReferenceAction, &QAction::toggled, this, [this](bool checked) {
        m_spectrumWidget->setShowReference(checked);
    });

    // Imported CSV traces
    m_importedTracesMenu = viewMenu->addMenu(tr("&Imported Traces"));
    m_importedTracesMenu->addAction(tr("(none)"))->setEnabled(false);

    // Interference monitoring
    viewMenu->addSeparator();
    m_interferenceAlertAction = viewMenu->addAction(tr("Enable &Interference Alerts"));
    m_interferenceAlertAction->setCheckable(true);
    m_interferenceAlertAction->setChecked(false);
    connect(m_interferenceAlertAction, &QAction::toggled, this, [this](bool checked) {
        m_interferenceMonitor->setEnabled(checked);
        if (checked && !m_interferenceMonitor->hasBaseline()) {
            // Auto-set baseline from current max-hold if available
            if (!m_session->isEmpty()) {
                m_interferenceMonitor->setBaseline(m_session->maxHold());
                statusBar()->showMessage(tr("Interference monitoring enabled — using current max-hold as baseline"), 3000);
            } else {
                statusBar()->showMessage(tr("Interference monitoring enabled — baseline will be set from first sweep data"), 3000);
            }
        }
    });

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
    m_deviceDock = new QDockWidget(tr("Device"), this);
    m_devicePanel = new DevicePanel(m_deviceManager);
    m_deviceDock->setWidget(m_devicePanel);
    m_deviceDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    addDockWidget(Qt::LeftDockWidgetArea, m_deviceDock);

    // Left dock: Capture controls (below device)
    m_captureDock = new QDockWidget(tr("Capture"), this);
    m_captureControls = new CaptureControls;
    m_captureDock->setWidget(m_captureControls);
    m_captureDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    addDockWidget(Qt::LeftDockWidgetArea, m_captureDock);

    // Stack device and capture docks vertically
    splitDockWidget(m_deviceDock, m_captureDock, Qt::Vertical);

    // Left dock: Export panel (below capture)
    m_exportDock = new QDockWidget(tr("Export Scan"), this);
    m_exportPanel = new ExportPanel;
    m_exportDock->setWidget(m_exportPanel);
    m_exportDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    addDockWidget(Qt::LeftDockWidgetArea, m_exportDock);
    splitDockWidget(m_captureDock, m_exportDock, Qt::Vertical);

    connect(m_exportPanel, &ExportPanel::exportRequested,
            this, &MainWindow::onExportFromPanel);

    // Right dock: Markers
    m_markerDock = new QDockWidget(tr("Markers"), this);
    m_markerPanel = new MarkerPanel;
    m_markerDock->setWidget(m_markerPanel);
    m_markerDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetClosable);
    addDockWidget(Qt::RightDockWidgetArea, m_markerDock);

    // Right dock: Detected Frequencies (below markers)
    m_frequencyListDock = new QDockWidget(tr("Detected Frequencies"), this);
    m_frequencyListPanel = new FrequencyListPanel;
    m_frequencyListDock->setWidget(m_frequencyListPanel);
    m_frequencyListDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetClosable);
    addDockWidget(Qt::RightDockWidgetArea, m_frequencyListDock);
    splitDockWidget(m_markerDock, m_frequencyListDock, Qt::Vertical);

    connect(m_frequencyListPanel, &FrequencyListPanel::frequencySelected,
            m_spectrumWidget, &SpectrumWidget::setHighlightFrequency);
    connect(m_frequencyListPanel, &FrequencyListPanel::frequencyDeselected,
            m_spectrumWidget, &SpectrumWidget::clearHighlight);
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

    // Auto-fit amplitude range after first full-resolution sweep
    m_amplitudeAutoFitPending = true;

    // Set frequency range on spectrum widget
    m_spectrumWidget->setFrequencyRange(startHz / 1e6, stopHz / 1e6);

    // Quick-start: if the user requests many more points than the device minimum,
    // do a fast low-resolution first sweep so data appears on screen immediately,
    // then reconfigure to full resolution.
    int quickPoints = device->minSweepPoints();
    if (points > quickPoints * 2) {
        m_quickStartPending = true;
        m_fullResStartHz = startHz;
        m_fullResStopHz = stopHz;
        m_fullResPoints = points;

        if (!device->configure(startHz, stopHz, quickPoints)) {
            statusBar()->showMessage(tr("Failed to configure device"));
            m_quickStartPending = false;
            return;
        }
    } else {
        m_quickStartPending = false;

        if (!device->configure(startHz, stopHz, points)) {
            statusBar()->showMessage(tr("Failed to configure device"));
            return;
        }
    }

    // Connect sweep signals
    connect(device, &ISpectrumDevice::sweepReady, this, &MainWindow::onSweepReady,
            Qt::UniqueConnection);
    connect(device, &ISpectrumDevice::partialSweepReady, this, [this](const SweepData& sweep) {
        if (m_captureControls->showLive())
            m_spectrumWidget->updateLive(sweep);
    }, Qt::UniqueConnection);
    connect(device, &ISpectrumDevice::sweepProgress, m_captureControls, &CaptureControls::onSweepProgress,
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
    m_quickStartPending = false;
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
    // Quick-start: the first sweep was a fast low-res preview.
    // Show it immediately, then reconfigure to the user's desired resolution.
    if (m_quickStartPending) {
        m_quickStartPending = false;

        // Display the preview sweep so the user sees data right away
        if (m_captureControls->showLive())
            m_spectrumWidget->updateLive(sweep);

        // Defer the reconfigure to the next event loop iteration.
        // We are inside a signal handler chain (onDataReady → processBuffer →
        // sweepReady → here), and sending serial commands synchronously can
        // cause the device to miss them because Qt cannot process incoming
        // serial I/O until this handler returns.
        QTimer::singleShot(0, this, [this]() {
            auto* device = m_deviceManager->currentDevice();
            if (device && device->isScanning()) {
                device->stopScanning();
                if (!device->configure(m_fullResStartHz, m_fullResStopHz, m_fullResPoints) ||
                    !device->startScanning()) {
                    statusBar()->showMessage(
                        tr("Failed to reconfigure for full resolution scan"), 5000);
                }
            }
        });
        return;
    }

    m_session->addSweep(sweep);

    if (m_captureControls->showLive())
        m_spectrumWidget->updateLive(sweep);
    if (m_captureControls->showMaxHold())
        m_spectrumWidget->updateMaxHold(m_session->maxHold());
    if (m_captureControls->showAverage())
        m_spectrumWidget->updateAverage(m_session->average());

    // Auto-fit Y-axis to actual signal range after first full-res sweep
    if (m_amplitudeAutoFitPending) {
        m_amplitudeAutoFitPending = false;
        m_spectrumWidget->autoFitAmplitude(sweep);
    }

    m_waterfallWidget->addSweep(sweep);
    m_captureControls->onSweepReceived();
    m_exportPanel->setExportEnabled(true);

    // Feed sweep to frequency list panel for peak detection
    m_frequencyListPanel->onSweepReceived(m_session->maxHold());

    // Check for interference against baseline
    m_interferenceMonitor->checkSweep(sweep);
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

void MainWindow::onExportFromPanel(const QString& format, const QString& dataSource, const QString& filePath)
{
    if (m_session->isEmpty()) {
        QMessageBox::information(this, tr("Export"), tr("No scan data to export. Run a scan first."));
        return;
    }

    if (filePath.isEmpty()) {
        return;
    }

    SweepData data;
    if (dataSource == "maxhold")
        data = m_session->maxHold();
    else if (dataSource == "average")
        data = m_session->average();
    else
        data = m_session->latestSweep();

    bool success = false;
    if (format == "wwb")
        success = WwbExporter::exportToFile(filePath, data);
    else
        success = GenericCsvExporter::exportToFile(filePath, data, m_session->deviceName());

    if (success)
        QMessageBox::information(this, tr("Export"), tr("Scan data exported successfully."));
    else
        QMessageBox::critical(this, tr("Export"), tr("Failed to write file."));
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

void MainWindow::onSaveSession()
{
    if (m_session->isEmpty()) {
        QMessageBox::information(this, tr("Save Session"), tr("No scan data to save. Run a scan first."));
        return;
    }

    QString filePath = QFileDialog::getSaveFileName(
        this, tr("Save Session"), QString(), tr("RFLogger Session (*.rfl);;All Files (*)"));

    if (filePath.isEmpty())
        return;

    if (!filePath.endsWith(".rfl", Qt::CaseInsensitive))
        filePath += ".rfl";

    if (SessionSerializer::save(filePath, *m_session))
        statusBar()->showMessage(tr("Session saved to %1").arg(filePath), 3000);
    else
        QMessageBox::critical(this, tr("Save Session"), tr("Failed to save session file."));
}

void MainWindow::onLoadReferenceTrace()
{
    QString filePath = QFileDialog::getOpenFileName(
        this, tr("Load Reference Trace"), QString(), tr("RFLogger Session (*.rfl);;All Files (*)"));

    if (filePath.isEmpty())
        return;

    auto result = SessionSerializer::load(filePath);
    if (!result.success) {
        QMessageBox::critical(this, tr("Load Reference"),
                              tr("Failed to load reference trace:\n%1").arg(result.errorMessage));
        return;
    }

    QString label = result.deviceName.isEmpty()
                        ? tr("Reference")
                        : tr("Ref: %1").arg(result.deviceName);

    m_spectrumWidget->setReferenceTrace(result.maxHold, label);
    m_showReferenceAction->setEnabled(true);
    m_showReferenceAction->setChecked(true);
    m_clearReferenceAction->setEnabled(true);

    // Use loaded reference as interference baseline if monitoring is enabled
    m_interferenceMonitor->setBaseline(result.maxHold);

    statusBar()->showMessage(
        tr("Reference trace loaded: %1 (%2 sweeps)")
            .arg(label).arg(result.sweepCount), 5000);
}

void MainWindow::onClearReferenceTrace()
{
    m_spectrumWidget->clearReferenceTrace();
    m_showReferenceAction->setEnabled(false);
    m_clearReferenceAction->setEnabled(false);
    statusBar()->showMessage(tr("Reference trace cleared"), 3000);
}

void MainWindow::onImportCsvTrace()
{
    QStringList filePaths = QFileDialog::getOpenFileNames(
        this, tr("Import CSV Traces"), QString(),
        tr("CSV Files (*.csv);;All Files (*)"));

    if (filePaths.isEmpty())
        return;

    int imported = 0;
    for (const auto& filePath : filePaths) {
        auto result = SessionSerializer::loadCsv(filePath);
        if (!result.success) {
            QMessageBox::warning(this, tr("Import CSV"),
                                 tr("Failed to import %1:\n%2")
                                     .arg(QFileInfo(filePath).fileName(), result.errorMessage));
            continue;
        }

        QString name = QFileInfo(filePath).completeBaseName();
        m_spectrumWidget->addImportedTrace(result.sweep, name);
        ++imported;
    }

    if (imported > 0) {
        rebuildImportedTracesMenu();
        statusBar()->showMessage(
            tr("Imported %1 CSV trace(s)").arg(imported), 3000);
    }
}

void MainWindow::rebuildImportedTracesMenu()
{
    m_importedTracesMenu->clear();

    const auto& traces = m_spectrumWidget->importedTraces();

    if (traces.isEmpty()) {
        m_importedTracesMenu->addAction(tr("(none)"))->setEnabled(false);
        return;
    }

    for (const auto& trace : traces) {
        auto* action = m_importedTracesMenu->addAction(trace.name);
        action->setCheckable(true);
        action->setChecked(trace.visible);
        int traceId = trace.id;
        connect(action, &QAction::toggled, this, [this, traceId](bool checked) {
            m_spectrumWidget->setImportedTraceVisible(traceId, checked);
        });
    }

    m_importedTracesMenu->addSeparator();

    m_importedTracesMenu->addAction(tr("Remove All Imported Traces"), this, [this]() {
        m_spectrumWidget->removeAllImportedTraces();
        rebuildImportedTracesMenu();
        statusBar()->showMessage(tr("All imported traces removed"), 3000);
    });
}

void MainWindow::updateSignalClassifier()
{
    SignalClassifier classifier;

    // Set TV channel map from current region
    QString tvRegion = "US (ATSC)";
    if (auto* checked = m_tvRegionGroup->checkedAction())
        tvRegion = checked->text();
    classifier.setTvChannelMap(TvChannelMap::forRegion(tvRegion));

    // Set band plan from current region
    QString bpRegion = "US";
    if (auto* checked = m_bandPlanRegionGroup->checkedAction())
        bpRegion = checked->text();
    classifier.setBandPlan(RfBandPlan::forRegion(bpRegion));

    m_frequencyListPanel->setSignalClassifier(classifier);
}

void MainWindow::updateDeviceLimits(ISpectrumDevice* device)
{
    connect(device, &ISpectrumDevice::deviceInfoUpdated, this, [this, device]() {
        // Update frequency limits only when the device has reported its range
        double minMHz = device->minFreqHz() / 1e6;
        double maxMHz = device->maxFreqHz() / 1e6;
        if (minMHz > 0 && maxMHz > 0)
            m_captureControls->setFrequencyLimits(minMHz, maxMHz);

        // Always recalculate sweep points — minSweepPoints/maxSweepPoints are
        // valid as soon as the device model is known (defaults to conservative
        // basic-model limits until PLUS capability is confirmed).
        m_captureControls->setSweepPointRange(device->minSweepPoints(), device->maxSweepPoints());

        double startHz = m_captureControls->startFreqMHz() * 1e6;
        double stopHz = m_captureControls->stopFreqMHz() * 1e6;
        int recommended = static_cast<int>(std::ceil((stopHz - startHz) / 10000.0)) + 1;
        recommended = std::max(recommended, device->minSweepPoints());
        recommended = std::min(recommended, device->maxSweepPoints());
        m_captureControls->setSweepPoints(recommended);
    });
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    auto* device = m_deviceManager->currentDevice();
    if (device && device->isScanning())
        onStopScan();

    saveSettings();

    // Shut down device manager while all widgets are still alive.
    // During destruction, ~QWidget() destroys child widgets before
    // ~QObject() destroys DeviceManager — any signal delivery between
    // those phases would hit destroyed widgets.
    m_deviceManager->shutdown();

    event->accept();
}

void MainWindow::toggleFullScreen()
{
    const QList<QDockWidget*> docks = {
        m_deviceDock, m_captureDock, m_exportDock,
        m_markerDock, m_frequencyListDock
    };

    if (!m_fullScreen) {
        // Save dock visibility before hiding
        m_dockVisibilityBeforeFullScreen.clear();
        for (auto* dock : docks)
            m_dockVisibilityBeforeFullScreen[dock] = dock->isVisible();

        for (auto* dock : docks)
            dock->setVisible(false);

        menuBar()->setVisible(false);
        statusBar()->setVisible(false);
        showFullScreen();
        m_fullScreen = true;

        // Show hint and start cursor-hide timer
        m_fullScreenHint->adjustSize();
        m_fullScreenHint->move((width() - m_fullScreenHint->width()) / 2, 32);
        m_fullScreenHint->raise();
        m_fullScreenHint->show();
        m_cursorHideTimer->start();
    } else {
        m_cursorHideTimer->stop();
        qApp->restoreOverrideCursor();
        m_fullScreenHint->hide();
        m_spectrumWidget->setCrosshairVisible(true);

        for (auto* dock : docks)
            dock->setVisible(m_dockVisibilityBeforeFullScreen.value(dock, true));

        menuBar()->setVisible(true);
        statusBar()->setVisible(true);
        showNormal();
        m_fullScreen = false;
    }

    if (m_fullScreenAction)
        m_fullScreenAction->setChecked(m_fullScreen);
}

bool MainWindow::eventFilter(QObject* obj, QEvent* event)
{
    if (m_fullScreen && event->type() == QEvent::MouseMove) {
        qApp->restoreOverrideCursor();
        m_spectrumWidget->setCrosshairVisible(true);
        m_fullScreenHint->move((width() - m_fullScreenHint->width()) / 2, 32);
        m_fullScreenHint->raise();
        m_fullScreenHint->show();
        m_cursorHideTimer->start();
    }
    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::keyPressEvent(QKeyEvent* event)
{
    if ((event->key() == Qt::Key_Escape || event->key() == Qt::Key_F11) && m_fullScreen) {
        toggleFullScreen();
        return;
    }
    QMainWindow::keyPressEvent(event);
}

void MainWindow::saveSettings()
{
    SettingsManager::saveWindowGeometry(this);
    SettingsManager::setValue("capture/startFreqMHz", m_captureControls->startFreqMHz());
    SettingsManager::setValue("capture/stopFreqMHz", m_captureControls->stopFreqMHz());
    SettingsManager::setValue("view/markersVisible", m_markerDock->isVisible());
    SettingsManager::setValue("view/frequencyListVisible", m_frequencyListDock->isVisible());
    SettingsManager::setValue("peakDetection/thresholdDb", m_frequencyListPanel->thresholdDb());
    SettingsManager::setValue("peakDetection/autoRefresh", m_frequencyListPanel->autoRefresh());
    SettingsManager::setValue("view/showTvBands", m_showTvBandsAction->isChecked());
    if (auto* checked = m_tvRegionGroup->checkedAction())
        SettingsManager::setValue("view/tvRegion", checked->text());
    SettingsManager::setValue("view/showBandPlan", m_showBandPlanAction->isChecked());
    if (auto* checked = m_bandPlanRegionGroup->checkedAction())
        SettingsManager::setValue("view/bandPlanRegion", checked->text());
    SettingsManager::setValue("view/interferenceAlerts", m_interferenceAlertAction->isChecked());
}

void MainWindow::loadSettings()
{
    SettingsManager::loadWindowGeometry(this);

    double startMHz = SettingsManager::value("capture/startFreqMHz", 470.0).toDouble();
    double stopMHz = SettingsManager::value("capture/stopFreqMHz", 700.0).toDouble();

    m_captureControls->setFrequencyRange(startMHz, stopMHz);
    m_spectrumWidget->setFrequencyRange(startMHz, stopMHz);

    bool markersVisible = SettingsManager::value("view/markersVisible", true).toBool();
    m_markerDock->setVisible(markersVisible);

    bool freqListVisible = SettingsManager::value("view/frequencyListVisible", true).toBool();
    m_frequencyListDock->setVisible(freqListVisible);

    double threshDb = SettingsManager::value("peakDetection/thresholdDb", 10.0).toDouble();
    m_frequencyListPanel->setThresholdDb(threshDb);

    bool autoRefresh = SettingsManager::value("peakDetection/autoRefresh", true).toBool();
    m_frequencyListPanel->setAutoRefresh(autoRefresh);

    // TV channel overlay
    QString tvRegion = SettingsManager::value("view/tvRegion", "US (ATSC)").toString();
    for (auto* action : m_tvRegionGroup->actions()) {
        if (action->text() == tvRegion) {
            action->setChecked(true);
            break;
        }
    }
    m_spectrumWidget->setTvChannelMap(TvChannelMap::forRegion(tvRegion));

    bool showTvBands = SettingsManager::value("view/showTvBands", false).toBool();
    m_showTvBandsAction->setChecked(showTvBands);

    // Band plan overlay
    QString bpRegion = SettingsManager::value("view/bandPlanRegion", "US").toString();
    for (auto* action : m_bandPlanRegionGroup->actions()) {
        if (action->text() == bpRegion) {
            action->setChecked(true);
            break;
        }
    }
    m_spectrumWidget->setBandPlanRegion(bpRegion);

    bool showBandPlan = SettingsManager::value("view/showBandPlan", false).toBool();
    m_showBandPlanAction->setChecked(showBandPlan);

    // Interference alerts
    bool alerts = SettingsManager::value("view/interferenceAlerts", false).toBool();
    m_interferenceAlertAction->setChecked(alerts);

    // Initialize signal classifier
    updateSignalClassifier();
}
