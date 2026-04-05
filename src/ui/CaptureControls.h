#pragma once

#include <QWidget>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QPushButton>
#include <QLabel>
#include <QCheckBox>
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

    bool showLive() const;
    bool showMaxHold() const;
    bool showAverage() const;

public slots:
    void onScanStarted();
    void onScanStopped();
    void onSweepReceived();

signals:
    void startScanRequested(double startHz, double stopHz, int points);
    void stopScanRequested();
    void showLiveChanged(bool show);
    void showMaxHoldChanged(bool show);
    void showAverageChanged(bool show);

private:
    QDoubleSpinBox* m_startFreqSpin;
    QDoubleSpinBox* m_stopFreqSpin;
    QSpinBox* m_pointsSpin;
    QPushButton* m_scanBtn;
    QLabel* m_sweepCountLabel;
    QLabel* m_sweepRateLabel;
    QLabel* m_elapsedLabel;
    QCheckBox* m_showLiveCheck;
    QCheckBox* m_showMaxHoldCheck;
    QCheckBox* m_showAverageCheck;

    QElapsedTimer m_elapsed;
    QTimer* m_uiTimer;
    int m_sweepCount = 0;
    bool m_scanning = false;
};
