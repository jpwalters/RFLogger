#include "CaptureControls.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>

CaptureControls::CaptureControls(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);

    // Frequency range group
    auto* freqGroup = new QGroupBox(tr("Frequency Range"));
    auto* freqLayout = new QFormLayout(freqGroup);

    m_startFreqSpin = new QDoubleSpinBox;
    m_startFreqSpin->setRange(0.1, 7000.0);
    m_startFreqSpin->setValue(470.0);
    m_startFreqSpin->setDecimals(3);
    m_startFreqSpin->setSuffix(" MHz");
    m_startFreqSpin->setSingleStep(1.0);
    freqLayout->addRow(tr("Start:"), m_startFreqSpin);

    m_stopFreqSpin = new QDoubleSpinBox;
    m_stopFreqSpin->setRange(0.1, 7000.0);
    m_stopFreqSpin->setValue(700.0);
    m_stopFreqSpin->setDecimals(3);
    m_stopFreqSpin->setSuffix(" MHz");
    m_stopFreqSpin->setSingleStep(1.0);
    freqLayout->addRow(tr("Stop:"), m_stopFreqSpin);

    m_pointsSpin = new QSpinBox;
    m_pointsSpin->setRange(112, 65535);
    m_pointsSpin->setValue(23001);
    freqLayout->addRow(tr("Points:"), m_pointsSpin);

    layout->addWidget(freqGroup);

    // Scan control
    auto* scanGroup = new QGroupBox(tr("Capture"));
    auto* scanLayout = new QVBoxLayout(scanGroup);

    m_scanBtn = new QPushButton(tr("▶  Start Scan"));
    m_scanBtn->setMinimumHeight(36);
    m_scanBtn->setStyleSheet(
        "QPushButton { background-color: #1a6b1a; border: 1px solid #2a8b2a; font-size: 13px; font-weight: bold; }"
        "QPushButton:hover { background-color: #228b22; }"
        "QPushButton:pressed { background-color: #145214; }");
    scanLayout->addWidget(m_scanBtn);

    auto* statsLayout = new QFormLayout;
    m_sweepCountLabel = new QLabel("0");
    statsLayout->addRow(tr("Sweeps:"), m_sweepCountLabel);

    m_sweepRateLabel = new QLabel("—");
    statsLayout->addRow(tr("Rate:"), m_sweepRateLabel);

    m_elapsedLabel = new QLabel("—");
    statsLayout->addRow(tr("Elapsed:"), m_elapsedLabel);

    scanLayout->addLayout(statsLayout);

    m_sweepProgress = new QProgressBar;
    m_sweepProgress->setRange(0, 100);
    m_sweepProgress->setValue(0);
    m_sweepProgress->setTextVisible(false);
    m_sweepProgress->setFixedHeight(8);
    m_sweepProgress->setVisible(false);
    m_sweepProgress->setStyleSheet(
        "QProgressBar { background-color: #2a2a2a; border: 1px solid #444; border-radius: 3px; }"
        "QProgressBar::chunk { background-color: #22cc55; border-radius: 2px; }");
    scanLayout->addWidget(m_sweepProgress);

    layout->addWidget(scanGroup);

    layout->addStretch();

    // Connections
    connect(m_scanBtn, &QPushButton::clicked, this, [this]() {
        if (m_scanning) {
            emit stopScanRequested();
        } else {
            emit startScanRequested(
                m_startFreqSpin->value() * 1'000'000.0,
                m_stopFreqSpin->value() * 1'000'000.0,
                m_pointsSpin->value());
        }
    });

    // UI update timer
    m_uiTimer = new QTimer(this);
    connect(m_uiTimer, &QTimer::timeout, this, [this]() {
        if (!m_scanning)
            return;
        qint64 ms = m_elapsed.elapsed();
        int secs = static_cast<int>(ms / 1000);
        int mins = secs / 60;
        secs %= 60;
        m_elapsedLabel->setText(QString("%1:%2").arg(mins, 2, 10, QChar('0')).arg(secs, 2, 10, QChar('0')));

        if (ms > 0) {
            double rate = m_sweepCount * 1000.0 / ms;
            m_sweepRateLabel->setText(QString("%1 /s").arg(rate, 0, 'f', 1));
        }

        // Time-based progress estimation when device sends no sub-sweep progress
        if (!m_gotDeviceProgress && m_lastSweepMs > 0) {
            qint64 sweepElapsed = m_sweepTimer.elapsed();
            int pct = static_cast<int>(sweepElapsed * 100 / m_lastSweepMs);
            pct = std::min(pct, 99);
            m_sweepProgress->setRange(0, 100);
            m_sweepProgress->setValue(pct);
        }
    });
}

double CaptureControls::startFreqMHz() const { return m_startFreqSpin->value(); }
double CaptureControls::stopFreqMHz() const { return m_stopFreqSpin->value(); }
int CaptureControls::sweepPoints() const { return m_pointsSpin->value(); }

bool CaptureControls::showLive() const { return m_showLive; }
bool CaptureControls::showMaxHold() const { return m_showMaxHold; }
bool CaptureControls::showAverage() const { return m_showAverage; }

void CaptureControls::setShowLive(bool show) { m_showLive = show; }
void CaptureControls::setShowMaxHold(bool show) { m_showMaxHold = show; }
void CaptureControls::setShowAverage(bool show) { m_showAverage = show; }

void CaptureControls::setFrequencyRange(double startMHz, double stopMHz)
{
    m_startFreqSpin->setValue(startMHz);
    m_stopFreqSpin->setValue(stopMHz);
}

void CaptureControls::setFrequencyLimits(double minMHz, double maxMHz)
{
    m_startFreqSpin->setRange(minMHz, maxMHz);
    m_stopFreqSpin->setRange(minMHz, maxMHz);
}

void CaptureControls::setSweepPointRange(int minPts, int maxPts)
{
    m_pointsSpin->setRange(minPts, maxPts);
}

void CaptureControls::setSweepPoints(int points)
{
    m_pointsSpin->setValue(points);
}

void CaptureControls::onScanStarted()
{
    m_scanning = true;
    m_sweepCount = 0;
    m_gotDeviceProgress = false;
    m_elapsed.start();
    m_sweepTimer.start();
    m_uiTimer->start(200);

    // Indeterminate (pulsing) until we have a timing reference or device progress
    if (m_lastSweepMs > 0) {
        m_sweepProgress->setRange(0, 100);
        m_sweepProgress->setValue(0);
    } else {
        m_sweepProgress->setRange(0, 0);  // Indeterminate mode
    }
    m_sweepProgress->setVisible(true);

    m_scanBtn->setText(tr("⏹  Stop Scan"));
    m_scanBtn->setStyleSheet(
        "QPushButton { background-color: #8b1a1a; border: 1px solid #ab2a2a; font-size: 13px; font-weight: bold; }"
        "QPushButton:hover { background-color: #aa2222; }"
        "QPushButton:pressed { background-color: #661414; }");

    m_startFreqSpin->setEnabled(false);
    m_stopFreqSpin->setEnabled(false);
    m_pointsSpin->setEnabled(false);
}

void CaptureControls::onScanStopped()
{
    m_scanning = false;
    m_uiTimer->stop();

    m_sweepProgress->setValue(0);
    m_sweepProgress->setVisible(false);

    m_scanBtn->setText(tr("▶  Start Scan"));
    m_scanBtn->setStyleSheet(
        "QPushButton { background-color: #1a6b1a; border: 1px solid #2a8b2a; font-size: 13px; font-weight: bold; }"
        "QPushButton:hover { background-color: #228b22; }"
        "QPushButton:pressed { background-color: #145214; }");

    m_startFreqSpin->setEnabled(true);
    m_stopFreqSpin->setEnabled(true);
    m_pointsSpin->setEnabled(true);
}

void CaptureControls::onSweepReceived()
{
    m_lastSweepMs = m_sweepTimer.elapsed();
    m_sweepTimer.start();
    m_gotDeviceProgress = false;

    m_sweepProgress->setRange(0, 100);
    m_sweepProgress->setValue(0);

    m_sweepCount++;
    m_sweepCountLabel->setText(QString::number(m_sweepCount));
}

void CaptureControls::onSweepProgress(int percent)
{
    m_gotDeviceProgress = true;
    m_sweepProgress->setRange(0, 100);
    m_sweepProgress->setValue(percent);
}
