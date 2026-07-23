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
    // Per-sweep ceiling for PLUS models. The SET_SWEEP_POINTS_LARGE ('Cj')
    // command can encode up to 65535, but real PLUS hardware (e.g. WSUB1G_PLUS)
    // sweeps such a large single request extremely slowly (~2 min/sweep) and
    // returns sparse, unusable data. 4096 is the hardware-validated value: it
    // keeps sweeps fast while staying granular (~44 kHz steps over 180 MHz).
    static constexpr int MAX_SWEEP_POINTS_PLUS = 4096;
    // Application-level ceiling for a single scan request. Kept equal to the
    // per-sweep device limit so the device is never asked for more points than
    // it can deliver in one fast, reliable sweep. Splitting a wide span into
    // multiple sub-band sweeps and stitching them was tried and reverted: the
    // PLUS hardware sweeps ~10 s per 4096-point band and keeps streaming the
    // OLD range for a sweep or two after each reconfigure, so a stitched scan
    // took minutes and captured stale/misaligned sub-bands. Detail across a
    // span is instead built up over time via Max-Hold of successive sweeps.
    static constexpr int MAX_ACCUMULATED_SWEEP_POINTS = 4096;

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
