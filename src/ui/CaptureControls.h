#pragma once

#include <QWidget>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QElapsedTimer>
#include <QTimer>

class CaptureControls : public QWidget
{
    Q_OBJECT

public:
    explicit CaptureControls(QWidget* parent = nullptr);

    double startFreqMHz() const;
    double stopFreqMHz() const;
    int sweepPoints() const;

    void setFrequencyRange(double startMHz, double stopMHz);
    void setFrequencyLimits(double minMHz, double maxMHz);
    void setSweepPointRange(int minPts, int maxPts);
    void setSweepPoints(int points);

    bool showLive() const;
    bool showMaxHold() const;
    bool showAverage() const;

    void setShowLive(bool show);
    void setShowMaxHold(bool show);
    void setShowAverage(bool show);

public slots:
    void onScanStarted();
    void onScanStopped();
    void onSweepReceived();
    void onSweepProgress(int percent);

signals:
    void startScanRequested(double startHz, double stopHz, int points);
    void stopScanRequested();

private:
    QDoubleSpinBox* m_startFreqSpin;
    QDoubleSpinBox* m_stopFreqSpin;
    QSpinBox* m_pointsSpin;
    QPushButton* m_scanBtn;
    QLabel* m_sweepCountLabel;
    QLabel* m_sweepRateLabel;
    QLabel* m_elapsedLabel;

    QProgressBar* m_sweepProgress;
    QElapsedTimer m_elapsed;
    QElapsedTimer m_sweepTimer;
    QTimer* m_uiTimer;
    int m_sweepCount = 0;
    qint64 m_lastSweepMs = 0;
    bool m_scanning = false;
    bool m_gotDeviceProgress = false;
    bool m_showLive = true;
    bool m_showMaxHold = true;
    bool m_showAverage = true;
};
