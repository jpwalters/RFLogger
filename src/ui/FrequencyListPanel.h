#pragma once

#include "data/DetectedSignal.h"
#include "data/PeakDetector.h"
#include "data/SignalClassifier.h"
#include "data/SweepData.h"

#include <QWidget>
#include <QTableWidget>
#include <QPushButton>
#include <QSlider>
#include <QLabel>
#include <QCheckBox>
#include <QVector>

class FrequencyListPanel : public QWidget
{
    Q_OBJECT

public:
    explicit FrequencyListPanel(QWidget* parent = nullptr);

    void setDemodulationAvailable(bool available);

    double thresholdDb() const;
    void setThresholdDb(double db);

    bool autoRefresh() const;
    void setAutoRefresh(bool on);

    void setSignalClassifier(const SignalClassifier& classifier);

public slots:
    void onSweepReceived(const SweepData& sweep);
    void detectNow(const SweepData& sweep);

signals:
    void listenRequested(double freqHz);
    void stopListenRequested();
    void frequencySelected(double freqHz);
    void frequencyDeselected();

private slots:
    void onDetectClicked();
    void onThresholdChanged(int value);
    void onListenClicked(int row);

private:
    void updateTable();

    QTableWidget* m_table;
    QSlider* m_thresholdSlider;
    QLabel* m_thresholdLabel;
    QCheckBox* m_autoRefreshBox;
    QPushButton* m_detectBtn;
    QPushButton* m_stopBtn;

    PeakDetector m_detector;
    SignalClassifier m_classifier;
    QVector<DetectedSignal> m_signals;
    SweepData m_lastSweep;
    bool m_demodAvailable = false;
    int m_listeningRow = -1;
};
