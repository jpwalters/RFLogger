#pragma once

#include "ISpectrumDevice.h"
#include "RtlSdrFft.h"

#include <QStringList>
#include <QThread>
#include <QVector>
#include <atomic>

struct rtlsdr_dev;
typedef struct rtlsdr_dev rtlsdr_dev_t;

class RtlSdrDevice : public ISpectrumDevice
{
    Q_OBJECT

public:
    explicit RtlSdrDevice(QObject* parent = nullptr);
    ~RtlSdrDevice() override;

    static QStringList availableDevices();

    // Returns empty string if driver is OK, or a user-facing message
    // if RTL-SDR hardware is detected but the WinUSB driver is missing.
    static QString checkDriverStatus();

    // ISpectrumDevice interface
    QString deviceName() const override;
    QString deviceType() const override { return QStringLiteral("RTL-SDR"); }

    bool connectDevice(const QString& portOrPath) override;
    void disconnectDevice() override;
    bool isConnected() const override;

    bool configure(double startFreqHz, double stopFreqHz, int sweepPoints) override;
    bool startScanning() override;
    void stopScanning() override;
    bool isScanning() const override;

    double minFreqHz() const override { return 25e6; }
    double maxFreqHz() const override { return 1.75e9; }
    int minSweepPoints() const override { return 128; }
    int maxSweepPoints() const override { return 8192; }

    QString modelName() const override;
    QString serialNumber() const override;

    // Gain control
    bool supportsGainControl() const override { return true; }
    QVector<int> availableGains() const override { return m_availableGains; }
    int currentGain() const override { return m_currentGain; }
    bool setGain(int tenthsDb) override;

private:
    void runSweepLoop();
    int snapToNearestGain(int tenthsDb) const;

    rtlsdr_dev_t* m_dev = nullptr;
    int m_deviceIndex = -1;
    QString m_deviceName;
    QString m_modelName;
    QString m_serialNumber;
    bool m_connected = false;

    // Gain
    QVector<int> m_availableGains;
    int m_currentGain = 0;

    // PPM correction
    int m_ppmCorrection = 0;

    // Scan configuration
    double m_startFreqHz = 0;
    double m_stopFreqHz = 0;
    int m_sweepPoints = 1024;

    // Worker thread
    QThread* m_workerThread = nullptr;
    std::atomic<bool> m_scanning{false};
    std::atomic<bool> m_stopRequested{false};

    // FFT
    static constexpr int FFT_SIZE = 1024;
    static constexpr double SAMPLE_RATE = 2.4e6;
    static constexpr double USABLE_BW_RATIO = 0.8;
    float m_decompandLut[256] = {};
};
