#pragma once

#include "ISpectrumDevice.h"

#include <QSerialPort>
#include <QByteArray>
#include <QTimer>

class RFExplorerDevice : public ISpectrumDevice
{
    Q_OBJECT

public:
    explicit RFExplorerDevice(QObject* parent = nullptr);
    ~RFExplorerDevice() override;

    QString deviceName() const override;
    QString deviceType() const override { return QStringLiteral("RF Explorer"); }

    bool connectDevice(const QString& portName) override;
    void disconnectDevice() override;
    bool isConnected() const override;

    bool configure(double startFreqHz, double stopFreqHz, int sweepPoints) override;
    bool startScanning() override;
    void stopScanning() override;
    bool isScanning() const override;

    double minFreqHz() const override { return m_minFreqHz; }
    double maxFreqHz() const override { return m_maxFreqHz; }
    int minSweepPoints() const override;
    int maxSweepPoints() const override;

    QString firmwareVersion() const override { return m_firmwareVersion; }
    QString serialNumber() const override { return m_serialNumber; }
    QString modelName() const override;

    // Test / diagnostic
    bool requestConfig();

private slots:
    void onDataReady();

private:
    static constexpr int BAUD_RATE = 500000;
    static constexpr int MIN_SWEEP_POINTS_BASIC = 112;
    static constexpr int MAX_SWEEP_POINTS_BASIC = 4096;
    static constexpr int MIN_SWEEP_POINTS_PLUS = 112;
    static constexpr int MAX_SWEEP_POINTS_PLUS = 4096;

    void sendCommand(const QByteArray& cmd);
    void processBuffer();
    void processConfigData(const QByteArray& data);
    void processScanData(const QByteArray& data, int sweepPoints);
    void processModelData(const QByteArray& data);
    void stripEosSequences(QByteArray& buffer);

    QSerialPort* m_serial = nullptr;
    QByteArray m_buffer;

    bool m_connected = false;
    bool m_scanning = false;
    bool m_disconnecting = false;
    bool m_configReceived = false;
    bool m_isPlusModel = false;

    // Device reported config
    double m_startFreqHz = 0.0;
    double m_freqStepHz = 0.0;
    int m_sweepPoints = 112;
    double m_minFreqHz = 0.0;
    double m_maxFreqHz = 0.0;
    double m_maxSpanHz = 0.0;

    // Desired config (from user)
    double m_desiredStartHz = 0.0;
    double m_desiredStopHz = 0.0;
    int m_desiredSweepPoints = 112;

    // Sub-sweep accumulation for high-resolution scans
    QVector<double> m_accumBuffer;
    double m_accumStartHz = 0.0;
    double m_accumStepHz = 0.0;
    int m_accumTarget = 0;
    int m_discardCount = 0;

    int m_mainModelCode = -1;
    int m_expansionModelCode = -1;
    QString m_firmwareVersion;
    QString m_serialNumber;
};
