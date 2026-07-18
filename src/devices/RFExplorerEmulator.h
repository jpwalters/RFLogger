#pragma once

#include <QIODevice>
#include <QByteArray>
#include <QTimer>

// Headless, in-memory emulator of an RF Explorer spectrum analyzer that speaks
// the RF Explorer UART protocol. It is a QIODevice so it can be attached to
// RFExplorerDevice via RFExplorerDevice::connectTransport() in place of a real
// serial port, allowing the scanning/parsing logic to be exercised without
// hardware.
//
// Behaviour is modelled on real devices per the UART API spec:
//   https://github.com/RFExplorer/RFExplorer-for-.NET/wiki/RF-Explorer-UART-API-interface-specification
//   - A single sweep is capped at the device's per-sweep hardware limit
//     (kMaxDevicePoints). Requests above that are clamped.
//   - Basic models snap sweep points to a multiple of 16.
//   - Each sweep covers the FULL configured span (the device does NOT sub-divide
//     a wide span into multiple stitched sub-sweeps).
class RFExplorerEmulator : public QIODevice
{
    Q_OBJECT

public:
    struct Model {
        int mainCode = 3;          // 3 = WSUB1G (basic). >=10 = PLUS.
        int expansionCode = 255;   // 255 = none
        QString firmware = "01.12";
        int minFreqKHz = 240000;   // 240 MHz
        int maxFreqKHz = 960000;   // 960 MHz
        int maxSpanKHz = 600000;   // 600 MHz
        int maxDevicePoints = 4096; // hardware per-sweep limit
        bool plus() const { return mainCode >= 10; }
    };

    // Convenience model resembling an RF Explorer PLUS (on-device high-res,
    // up to 65535 points per sweep) covering a wide range — handy for the demo.
    static Model plusDemoModel()
    {
        Model m;
        m.mainCode = 13;          // 4G_PLUS
        m.expansionCode = 255;
        m.firmware = "03.10";
        m.minFreqKHz = 50000;     // 50 MHz
        m.maxFreqKHz = 4000000;   // 4 GHz
        m.maxSpanKHz = 600000;
        m.maxDevicePoints = 65535;
        return m;
    }

    explicit RFExplorerEmulator(const Model& model, QObject* parent = nullptr);

    // Interval between streamed sweeps. Tests use a very small value for speed;
    // the live demo uses a hardware-like cadence so the UI looks realistic.
    void setSweepInterval(int ms);

    // QIODevice
    bool isSequential() const override { return true; }
    bool open(OpenMode mode) override;
    qint64 bytesAvailable() const override;

    // Diagnostics for tests
    int lastRequestedPoints() const { return m_lastRequestedPoints; }
    int configuredPoints() const { return m_points; }
    bool isStreaming() const { return m_timer && m_timer->isActive(); }
    int sweepsSent() const { return m_sweepsSent; }

protected:
    qint64 readData(char* data, qint64 maxSize) override;
    qint64 writeData(const char* data, qint64 maxSize) override;

private:
    void handleCommand(const QByteArray& cmd);
    void sendModelData();
    void sendConfigData();
    void startStreaming();
    void stopStreaming();
    void sendSweep();
    void queue(const QByteArray& bytes);
    void scheduleReadyRead();

    Model m_model;

    QByteArray m_inbound;   // commands received from the host
    QByteArray m_outbound;  // bytes waiting to be read by the host

    // Current device configuration
    int m_startKHz = 240000;
    int m_stopKHz = 260000;
    int m_points = 112;             // effective (clamped/snapped) sweep points
    int m_lastRequestedPoints = 112; // raw value last requested by the host

    QTimer* m_timer = nullptr;
    bool m_emitScheduled = false;
    int m_sweepsSent = 0;
};
