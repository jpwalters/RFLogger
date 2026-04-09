#pragma once

#include "ISpectrumDevice.h"

#include <QSerialPort>
#include <QByteArray>

class TinySADevice : public ISpectrumDevice
{
    Q_OBJECT

public:
    explicit TinySADevice(QObject* parent = nullptr);
    ~TinySADevice() override;

    QString deviceName() const override;
    QString deviceType() const override { return QStringLiteral("TinySA"); }

    bool connectDevice(const QString& portName) override;
    void disconnectDevice() override;
    bool isConnected() const override;

    bool configure(double startFreqHz, double stopFreqHz, int sweepPoints) override;
    bool startScanning() override;
    void stopScanning() override;
    bool isScanning() const override;

    double minFreqHz() const override;
    double maxFreqHz() const override;
    int minSweepPoints() const override;
    int maxSweepPoints() const override;

    QString firmwareVersion() const override { return m_hwVersion; }
    QString modelName() const override;

private slots:
    void onDataReady();

private:
    enum class Model { Unknown, Basic, Ultra };
    enum class FreqMode { Low, High };

    static constexpr int BAUD_RATE = 115200;

    // Amplitude decoding constants (TinySA binary protocol)
    static constexpr int BYTES_PER_POINT = 3;
    static constexpr double AMPLITUDE_SCALE = 32.0;
    static constexpr double AMPLITUDE_OFFSET_BASIC = 128.0;
    static constexpr double AMPLITUDE_OFFSET_ULTRA = 174.0;

    // Basic ranges
    static constexpr double MIN_FREQ_BASIC_LOW  =    100000.0;
    static constexpr double MAX_FREQ_BASIC_LOW  = 350000000.0;
    static constexpr double MIN_FREQ_BASIC_HIGH = 240000000.0;
    static constexpr double MAX_FREQ_BASIC_HIGH = 960000000.0;

    // Ultra range
    static constexpr double MIN_FREQ_ULTRA =       100000.0;
    static constexpr double MAX_FREQ_ULTRA = 6000000000.0;

    void sendTextCommand(const QString& cmd);
    void processResponse();
    void processIncrementalScan();
    void processVersionResponse(const QStringList& lines);
    void processSweepConfigResponse(const QStringList& lines);
    void processScanData(const QByteArray& rawData);
    QVector<double> parseScanAmplitudes(const char* data, int dataSize, int count) const;
    void requestVersion();
    void requestSweepConfig();
    void startScanRaw();

    QSerialPort* m_serial = nullptr;
    QByteArray m_buffer;

    bool m_connected = false;
    bool m_scanning = false;
    bool m_disconnecting = false;
    bool m_configReceived = false;
    Model m_model = Model::Unknown;
    FreqMode m_freqMode = FreqMode::High;

    QString m_lastCommand;
    QString m_hwVersion;

    double m_startFreqHz = 0.0;
    double m_stopFreqHz = 0.0;
    int m_sweepPoints = 450;

    // Incremental scan parsing
    bool m_scanDataStarted = false;
    int m_scanDataOffset = 0;
    int m_parsedScanPoints = 0;
    static constexpr int PARTIAL_SCAN_THRESHOLD = 50;
};
