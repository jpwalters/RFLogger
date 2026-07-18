#pragma once

#include "ISpectrumDevice.h"

#include <QSerialPort>
#include <QIODevice>
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
    int maxAccumulatedSweepPoints() const override { return MAX_ACCUMULATED_SWEEP_POINTS; }

    QString firmwareVersion() const override { return m_firmwareVersion; }
    QString serialNumber() const override { return m_serialNumber; }
    QString modelName() const override;

    // Test / diagnostic
    bool requestConfig();

    // Test seam: attach an already-open QIODevice (e.g. an in-memory emulator)
    // as the transport instead of opening a real serial port. Takes ownership
    // of \a transport (it is re-parented to this device). Returns true on success.
    bool connectTransport(QIODevice* transport);

private slots:
    void onDataReady();
    void processDeferredBuffer();

private:
    static constexpr int BAUD_RATE = 500000;
    static constexpr double AMPLITUDE_DIVISOR = -2.0;  // dBm = raw_byte / -2.0
    static constexpr int MIN_SWEEP_POINTS_BASIC = 112;
    static constexpr int MAX_SWEEP_POINTS_BASIC = 4096;
    static constexpr int MIN_SWEEP_POINTS_PLUS = 112;
    // PLUS models render high resolution on the device: up to 65535 points in a
    // single sweep via SET_SWEEP_POINTS_LARGE ('Cj') + '$z' scan data. This is
    // the documented hardware ceiling per the RF Explorer UART API.
    static constexpr int MAX_SWEEP_POINTS_PLUS = 65535;
    // Application-level ceiling for high-resolution scans. For PLUS models this
    // matches the device's native single-sweep limit; for basic models it is the
    // ceiling when stitching multiple sub-band sweeps together.
    static constexpr int MAX_ACCUMULATED_SWEEP_POINTS = 65535;

    void sendCommand(const QByteArray& cmd);
    bool attachTransport(QIODevice* transport);
    void flushTransport();
    bool transportHasError() const;
    void processBuffer(bool isDeferred = false);
    void processConfigData(const QByteArray& data);
    void processScanData(const QByteArray& data, int sweepPoints);
    void processModelData(const QByteArray& data);
    void stripEosSequences(QByteArray& buffer);

    QIODevice* m_serial = nullptr;
    QByteArray m_buffer;

    bool m_connected = false;
    bool m_scanning = false;
    bool m_disconnecting = false;
    bool m_configReceived = false;
    bool m_isPlusModel = false;
    bool m_processBufferScheduled = false;

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
