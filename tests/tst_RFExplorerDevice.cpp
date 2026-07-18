#include <QtTest>
#include <QSignalSpy>
#include <QCoreApplication>

#include "devices/RFExplorerDevice.h"
#include "data/SweepData.h"
#include "devices/RFExplorerEmulator.h"

Q_DECLARE_METATYPE(SweepData)

// Reproduces the RF Explorer scanning flow against the in-memory emulator so the
// "one fast sweep then nothing" / high-resolution behaviour can be diagnosed
// without hardware. The sequence mirrors MainWindow::onStartScan +
// onSweepReady quick-start reconfigure.
class tst_RFExplorerDevice : public QObject
{
    Q_OBJECT

private:
    // Drive the connect + initial config handshake, returning a connected device.
    static void connectAndWait(RFExplorerDevice& dev, RFExplorerEmulator* emu)
    {
        QVERIFY(dev.connectTransport(emu));
        // Wait for the initial config so device frequency bounds are populated.
        QTRY_VERIFY_WITH_TIMEOUT(dev.maxFreqHz() > 0.0, 2000);
    }

private slots:
    void initTestCase()
    {
        qRegisterMetaType<SweepData>("SweepData");
    }

    void basicModel_quickStartThenHighRes()
    {
        RFExplorerDevice dev;
        auto* emu = new RFExplorerEmulator(RFExplorerEmulator::Model{}); // basic WSUB1G
        connectAndWait(dev, emu);

        const double startHz = 470e6;
        const double stopHz = 700e6;
        const int fullPoints = 23001;
        const int quickPoints = dev.minSweepPoints();

        QSignalSpy sweepSpy(&dev, &ISpectrumDevice::sweepReady);
        QSignalSpy partialSpy(&dev, &ISpectrumDevice::partialSweepReady);

        // Quick-start: low-res sweep first.
        QVERIFY(dev.configure(startHz, stopHz, quickPoints));
        QVERIFY(dev.startScanning());
        QTRY_VERIFY_WITH_TIMEOUT(sweepSpy.count() >= 1, 3000);

        const int sweepsAfterQuick = sweepSpy.count();
        qInfo() << "Quick-start full sweeps:" << sweepsAfterQuick
                << "partials:" << partialSpy.count();

        // Reconfigure to full resolution — exactly what MainWindow does.
        dev.stopScanning();
        QVERIFY(dev.configure(startHz, stopHz, fullPoints));
        QVERIFY(dev.startScanning());

        // Observe for a while.
        QTest::qWait(800);

        qInfo() << "After high-res reconfigure — emulator streaming:" << emu->isStreaming()
                << "emulator sweeps sent:" << emu->sweepsSent()
                << "emulator configured points:" << emu->configuredPoints()
                << "| app full sweeps:" << sweepSpy.count()
                << "app partials:" << partialSpy.count();

        // The emulator should keep streaming after reconfigure.
        QVERIFY2(emu->isStreaming(), "emulator stopped streaming after reconfigure");
        QVERIFY2(emu->sweepsSent() > sweepsAfterQuick, "device stopped producing sweeps after reconfigure");

        // The app must keep producing full sweeps after reconfigure — the
        // "one fast sweep then nothing" regression.
        QVERIFY2(sweepSpy.count() > sweepsAfterQuick,
                 "no full sweeps produced after high-res reconfigure");

        // The frequency axis of the emitted sweeps must match the requested
        // 470-700 MHz range. The known-good behaviour stitches multiple device
        // sub-sweeps into the requested high-resolution point count while
        // keeping the axis pinned to the requested span.
        const SweepData s = qvariant_cast<SweepData>(sweepSpy.last().at(0));
        qInfo() << "Last full sweep: points=" << s.count()
                << "start(MHz)=" << s.startFreqHz() / 1e6
                << "stop(MHz)=" << s.stopFreqHz() / 1e6;
        QCOMPARE(s.startFreqHz() / 1e6, 470.0);
        QVERIFY2(qAbs(s.stopFreqHz() / 1e6 - 700.0) < 1.0,
                 "stitched sweep stretched the frequency axis beyond the requested span");
        QCOMPARE(s.count(), fullPoints);
    }

    void plusModel_requestedPointsSentToDevice()
    {
        RFExplorerDevice dev;
        RFExplorerEmulator::Model plus;
        plus.mainCode = 13;          // 4G_PLUS
        plus.firmware = "03.10";
        plus.minFreqKHz = 50000;     // 50 MHz
        plus.maxFreqKHz = 4000000;   // 4 GHz
        plus.maxDevicePoints = 65535; // PLUS native single-sweep limit
        auto* emu = new RFExplorerEmulator(plus);
        connectAndWait(dev, emu);

        QVERIFY(dev.deviceName().contains("PLUS"));

        // Request a high-resolution point count above the device per-sweep limit.
        QVERIFY(dev.configure(470e6, 700e6, 23001));
        QTest::qWait(50);

        qInfo() << "PLUS: host requested points ->" << emu->lastRequestedPoints()
                << "device max ->" << 65535
                << "device configured ->" << emu->configuredPoints();

        // PLUS renders high resolution on the device: the full requested point
        // count (up to 65535) must reach the device unclamped so the user gets
        // the detailed scan they asked for.
        QCOMPARE(emu->lastRequestedPoints(), 23001);
        QCOMPARE(emu->configuredPoints(), 23001);
    }
};

QTEST_GUILESS_MAIN(tst_RFExplorerDevice)
#include "tst_RFExplorerDevice.moc"
