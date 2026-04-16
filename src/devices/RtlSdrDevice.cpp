#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#include <SetupAPI.h>
#include <cfgmgr32.h>
#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "cfgmgr32.lib")
#endif

#include "RtlSdrDevice.h"
#include "app/SettingsManager.h"

#include <rtl-sdr.h>

#include <QDebug>
#include <QDateTime>
#include <cmath>
#include <algorithm>

RtlSdrDevice::RtlSdrDevice(QObject* parent)
    : ISpectrumDevice(parent)
{
    RtlSdrFft::buildDecompandingLut(m_decompandLut);
}

RtlSdrDevice::~RtlSdrDevice()
{
    disconnectDevice();
}

QStringList RtlSdrDevice::availableDevices()
{
    QStringList devices;
    int count = rtlsdr_get_device_count();
    for (int i = 0; i < count; ++i) {
        devices << QStringLiteral("rtlsdr:%1").arg(i);
    }
    return devices;
}

QString RtlSdrDevice::checkDriverStatus()
{
#ifdef _WIN32
    // If librtlsdr already sees the device, the driver is fine.
    if (rtlsdr_get_device_count() > 0)
        return {};

    // Enumerate ALL present USB devices via SetupAPI to find driverless RTL2832U.
    HDEVINFO devInfo = SetupDiGetClassDevsW(
        nullptr, L"USB", nullptr, DIGCF_ALLCLASSES | DIGCF_PRESENT);
    if (devInfo == INVALID_HANDLE_VALUE)
        return {};

    // Known RTL-SDR VID/PID combinations
    static const wchar_t* rtlHwIds[] = {
        L"VID_0BDA&PID_2832",
        L"VID_0BDA&PID_2838",
    };

    SP_DEVINFO_DATA devInfoData{};
    devInfoData.cbSize = sizeof(devInfoData);

    bool foundDriverless = false;
    QString deviceFriendlyName;

    for (DWORD i = 0; SetupDiEnumDeviceInfo(devInfo, i, &devInfoData); ++i) {
        // Get hardware ID
        wchar_t hwIdBuf[512] = {};
        if (!SetupDiGetDeviceRegistryPropertyW(devInfo, &devInfoData,
                SPDRP_HARDWAREID, nullptr,
                reinterpret_cast<PBYTE>(hwIdBuf), sizeof(hwIdBuf), nullptr)) {
            continue;
        }

        // Check if any known RTL-SDR VID/PID matches
        bool isRtlSdr = false;
        for (const auto* id : rtlHwIds) {
            if (wcsstr(hwIdBuf, id)) {
                isRtlSdr = true;
                break;
            }
        }
        if (!isRtlSdr)
            continue;

        // Found an RTL-SDR — check if it has a working driver
        DWORD status = 0, problem = 0;
        if (CM_Get_DevNode_Status(&status, &problem,
                devInfoData.DevInst, 0) == CR_SUCCESS) {
            if (problem != 0) {
                foundDriverless = true;

                // Get friendly name for display
                wchar_t nameBuf[256] = {};
                if (SetupDiGetDeviceRegistryPropertyW(devInfo, &devInfoData,
                        SPDRP_FRIENDLYNAME, nullptr,
                        reinterpret_cast<PBYTE>(nameBuf), sizeof(nameBuf), nullptr)) {
                    deviceFriendlyName = QString::fromWCharArray(nameBuf);
                } else if (SetupDiGetDeviceRegistryPropertyW(devInfo, &devInfoData,
                        SPDRP_DEVICEDESC, nullptr,
                        reinterpret_cast<PBYTE>(nameBuf), sizeof(nameBuf), nullptr)) {
                    deviceFriendlyName = QString::fromWCharArray(nameBuf);
                }
                break;
            }
        }
    }

    SetupDiDestroyDeviceInfoList(devInfo);

    if (foundDriverless) {
        QString name = deviceFriendlyName.isEmpty()
            ? QStringLiteral("RTL-SDR") : deviceFriendlyName;
        return QStringLiteral(
            "%1 detected but the WinUSB driver is not installed.\n"
            "Run drivers/install_rtlsdr_driver.bat as Administrator,\n"
            "or use Zadig (zadig.akeo.ie) to install the WinUSB driver.")
            .arg(name);
    }
#endif
    return {};
}

QString RtlSdrDevice::deviceName() const
{
    return QStringLiteral("RTL-SDR");
}

QString RtlSdrDevice::modelName() const
{
    return m_modelName.isEmpty() ? deviceName() : m_modelName;
}

QString RtlSdrDevice::serialNumber() const
{
    return m_serialNumber;
}

bool RtlSdrDevice::connectDevice(const QString& portOrPath)
{
    if (m_connected)
        disconnectDevice();

    // Parse device index from "rtlsdr:N"
    if (!portOrPath.startsWith("rtlsdr:")) {
        emit errorOccurred(tr("Invalid RTL-SDR path: %1").arg(portOrPath));
        return false;
    }

    bool ok = false;
    m_deviceIndex = portOrPath.mid(7).toInt(&ok);
    if (!ok || m_deviceIndex < 0) {
        emit errorOccurred(tr("Invalid RTL-SDR device index: %1").arg(portOrPath));
        return false;
    }

    int deviceCount = rtlsdr_get_device_count();
    if (m_deviceIndex >= deviceCount) {
        emit errorOccurred(tr("RTL-SDR device %1 not found (%2 devices available)")
                               .arg(m_deviceIndex).arg(deviceCount));
        return false;
    }

    // Get device info
    char manufact[256] = {};
    char product[256] = {};
    char serial[256] = {};
    rtlsdr_get_device_usb_strings(m_deviceIndex, manufact, product, serial);
    m_modelName = QStringLiteral("%1 %2").arg(QString::fromLatin1(manufact).trimmed(),
                                              QString::fromLatin1(product).trimmed());
    m_serialNumber = QString::fromLatin1(serial).trimmed();
    m_deviceName = QStringLiteral("RTL-SDR (%1)").arg(
        rtlsdr_get_device_name(m_deviceIndex));

    // Open device
    int ret = rtlsdr_open(&m_dev, static_cast<uint32_t>(m_deviceIndex));
    if (ret < 0) {
        emit errorOccurred(tr("Failed to open RTL-SDR device %1 (error %2)")
                               .arg(m_deviceIndex).arg(ret));
        m_dev = nullptr;
        return false;
    }

    // Set sample rate
    ret = rtlsdr_set_sample_rate(m_dev, static_cast<uint32_t>(SAMPLE_RATE));
    if (ret < 0) {
        qWarning() << "RTL-SDR: failed to set sample rate" << ret;
    }

    // Manual gain mode
    rtlsdr_set_tuner_gain_mode(m_dev, 1);

    // Query available gains
    int numGains = rtlsdr_get_tuner_gains(m_dev, nullptr);
    if (numGains > 0) {
        m_availableGains.resize(numGains);
        rtlsdr_get_tuner_gains(m_dev, m_availableGains.data());
        std::sort(m_availableGains.begin(), m_availableGains.end());

        // Default to mid-range gain
        int midIndex = numGains / 2;
        m_currentGain = m_availableGains[midIndex];
        rtlsdr_set_tuner_gain(m_dev, m_currentGain);
    }

    // PPM correction (persisted in settings, default 0 for TCXO dongles)
    m_ppmCorrection = SettingsManager::value("rtlsdr/ppmCorrection", 0).toInt();
    if (m_ppmCorrection != 0) {
        rtlsdr_set_freq_correction(m_dev, m_ppmCorrection);
    }

    // Reset buffer
    rtlsdr_reset_buffer(m_dev);

    m_connected = true;
    emit connectionChanged(true);
    emit deviceInfoUpdated();

    qDebug() << "RTL-SDR connected:" << m_modelName << "serial:" << m_serialNumber
             << "gains:" << m_availableGains.size() << "steps, PPM:" << m_ppmCorrection;

    return true;
}

void RtlSdrDevice::disconnectDevice()
{
    stopScanning();

    if (m_dev) {
        rtlsdr_close(m_dev);
        m_dev = nullptr;
    }

    if (m_connected) {
        m_connected = false;
        m_availableGains.clear();
        m_currentGain = 0;
        emit connectionChanged(false);
    }
}

bool RtlSdrDevice::isConnected() const
{
    return m_connected;
}

bool RtlSdrDevice::configure(double startFreqHz, double stopFreqHz, int sweepPoints)
{
    if (!m_connected)
        return false;

    m_startFreqHz = startFreqHz;
    m_stopFreqHz = stopFreqHz;
    m_sweepPoints = sweepPoints;
    return true;
}

bool RtlSdrDevice::startScanning()
{
    if (!m_connected || m_scanning.load())
        return false;

    m_stopRequested.store(false);
    m_scanning.store(true);

    m_workerThread = QThread::create([this]() { runSweepLoop(); });
    m_workerThread->start();

    return true;
}

void RtlSdrDevice::stopScanning()
{
    if (!m_scanning.load())
        return;

    m_stopRequested.store(true);

    if (m_workerThread) {
        m_workerThread->wait(5000);
        delete m_workerThread;
        m_workerThread = nullptr;
    }

    m_scanning.store(false);
}

bool RtlSdrDevice::isScanning() const
{
    return m_scanning.load();
}

bool RtlSdrDevice::setGain(int tenthsDb)
{
    if (!m_connected || m_availableGains.isEmpty())
        return false;

    int snapped = snapToNearestGain(tenthsDb);
    int ret = rtlsdr_set_tuner_gain(m_dev, snapped);
    if (ret < 0)
        return false;

    m_currentGain = snapped;
    emit gainChanged(m_currentGain);
    return true;
}

int RtlSdrDevice::snapToNearestGain(int tenthsDb) const
{
    if (m_availableGains.isEmpty())
        return tenthsDb;

    int closest = m_availableGains.first();
    int minDist = std::abs(tenthsDb - closest);
    for (int g : m_availableGains) {
        int dist = std::abs(tenthsDb - g);
        if (dist < minDist) {
            minDist = dist;
            closest = g;
        }
    }
    return closest;
}

void RtlSdrDevice::runSweepLoop()
{
    RtlSdrFft fft(FFT_SIZE);
    const double usableBw = SAMPLE_RATE * USABLE_BW_RATIO;
    const double totalSpan = m_stopFreqHz - m_startFreqHz;

    // Number of tuning segments needed to cover the requested span
    int numSegments = std::max(1, static_cast<int>(std::ceil(totalSpan / usableBw)));

    // Points per segment that contribute to the final sweep
    // We discard 10% on each edge of the FFT output
    const int discardBins = static_cast<int>(FFT_SIZE * (1.0 - USABLE_BW_RATIO) / 2.0);
    const int usableBins = FFT_SIZE - 2 * discardBins;

    // Recompute step size to distribute points evenly across the span
    const double stepSizeHz = totalSpan / (numSegments * usableBins - 1);

    // Total output points
    const int totalPoints = numSegments * usableBins;

    // I/Q read buffer: 2 bytes per complex sample (I, Q)
    const int readLen = FFT_SIZE * 2;
    QVector<unsigned char> iqBuf(readLen);

    QVector<double> sweepAmplitudes;
    sweepAmplitudes.reserve(totalPoints);

    while (!m_stopRequested.load()) {
        sweepAmplitudes.clear();

        for (int seg = 0; seg < numSegments; ++seg) {
            if (m_stopRequested.load())
                break;

            // Compute center frequency for this segment
            double segStartFreq = m_startFreqHz + seg * usableBins * stepSizeHz;
            double segCenterFreq = segStartFreq + (usableBins / 2.0) * stepSizeHz;

            // Tune to center frequency
            rtlsdr_set_center_freq(m_dev, static_cast<uint32_t>(segCenterFreq));

            // Brief settle time for PLL lock
            // Read and discard one buffer to flush stale samples
            int bytesRead = 0;
            rtlsdr_read_sync(m_dev, iqBuf.data(), readLen, &bytesRead);

            // Read actual data
            bytesRead = 0;
            int ret = rtlsdr_read_sync(m_dev, iqBuf.data(), readLen, &bytesRead);
            if (ret < 0 || bytesRead < readLen) {
                // Device read error — may have been unplugged
                if (!m_stopRequested.load()) {
                    m_scanning.store(false);
                    QMetaObject::invokeMethod(this, [this]() {
                        emit errorOccurred(tr("RTL-SDR read error — device may have been disconnected"));
                        disconnectDevice();
                    }, Qt::QueuedConnection);
                }
                return;
            }

            // FFT → power spectrum
            QVector<double> segmentDbm;
            fft.computePowerSpectrum(iqBuf.data(), m_decompandLut, segmentDbm);

            // Extract usable center bins (discard edges)
            for (int i = discardBins; i < FFT_SIZE - discardBins; ++i) {
                sweepAmplitudes.append(segmentDbm[i]);
            }

            // Emit partial sweep progress
            int percent = static_cast<int>((seg + 1) * 100.0 / numSegments);
            QMetaObject::invokeMethod(this, [this, percent]() {
                emit sweepProgress(percent);
            }, Qt::QueuedConnection);

            // Emit partial sweep for real-time display
            if (seg < numSegments - 1) {
                SweepData partial(m_startFreqHz, stepSizeHz,
                                  sweepAmplitudes, QDateTime::currentDateTimeUtc());
                QMetaObject::invokeMethod(this, [this, partial]() {
                    emit partialSweepReady(partial);
                }, Qt::QueuedConnection);
            }
        }

        if (m_stopRequested.load())
            break;

        // Trim or pad to requested sweep points
        if (sweepAmplitudes.size() > m_sweepPoints) {
            sweepAmplitudes.resize(m_sweepPoints);
        }

        // Emit complete sweep
        double finalStepSize = totalSpan / std::max(qsizetype(1), sweepAmplitudes.size() - 1);
        SweepData sweep(m_startFreqHz, finalStepSize,
                        sweepAmplitudes, QDateTime::currentDateTimeUtc());

        QMetaObject::invokeMethod(this, [this, sweep]() {
            emit sweepReady(sweep);
        }, Qt::QueuedConnection);
    }

    m_scanning.store(false);
}
