#include "TinySADevice.h"

#include <QDebug>
#include <QThread>
#include <QTimer>
#include <algorithm>
#include <cmath>

// TinySA API: https://tinysa.org/wiki/pmwiki.php?n=Main.USBInterface
// Protocol reference: berkon/wireless-microphone-analyzer scan_devices/tiny_sa.js

TinySADevice::TinySADevice(QObject* parent)
    : ISpectrumDevice(parent)
{
}

TinySADevice::~TinySADevice()
{
    disconnectDevice();
}

QString TinySADevice::deviceName() const
{
    switch (m_model) {
    case Model::Basic: return QStringLiteral("TinySA Basic");
    case Model::Ultra: return QStringLiteral("TinySA Ultra");
    default: return QStringLiteral("TinySA");
    }
}

QString TinySADevice::modelName() const
{
    return deviceName();
}

double TinySADevice::minFreqHz() const
{
    switch (m_model) {
    case Model::Basic:
        return m_freqMode == FreqMode::Low ? MIN_FREQ_BASIC_LOW : MIN_FREQ_BASIC_HIGH;
    case Model::Ultra:
        return MIN_FREQ_ULTRA;
    default:
        return MIN_FREQ_BASIC_HIGH;
    }
}

double TinySADevice::maxFreqHz() const
{
    switch (m_model) {
    case Model::Basic:
        return m_freqMode == FreqMode::Low ? MAX_FREQ_BASIC_LOW : MAX_FREQ_BASIC_HIGH;
    case Model::Ultra:
        return MAX_FREQ_ULTRA;
    default:
        return MAX_FREQ_BASIC_HIGH;
    }
}

int TinySADevice::minSweepPoints() const
{
    return m_model == Model::Ultra ? 25 : 51;
}

int TinySADevice::maxSweepPoints() const
{
    return 65535;
}

bool TinySADevice::connectDevice(const QString& portName)
{
    if (m_connected)
        disconnectDevice();

    m_serial = new QSerialPort(this);
    m_serial->setPortName(portName);
    m_serial->setBaudRate(BAUD_RATE);
    m_serial->setDataBits(QSerialPort::Data8);
    m_serial->setParity(QSerialPort::NoParity);
    m_serial->setStopBits(QSerialPort::OneStop);
    m_serial->setFlowControl(QSerialPort::NoFlowControl);

    if (!m_serial->open(QIODevice::ReadWrite)) {
        emit errorOccurred(tr("Failed to open %1: %2").arg(portName, m_serial->errorString()));
        delete m_serial;
        m_serial = nullptr;
        return false;
    }

    connect(m_serial, &QSerialPort::readyRead, this, &TinySADevice::onDataReady);
    connect(m_serial, &QSerialPort::errorOccurred, this, [this](QSerialPort::SerialPortError error) {
        if (error == QSerialPort::ResourceError && !m_disconnecting) {
            qDebug() << "TinySA: device removed (ResourceError)";
            // Mark as disconnecting immediately to stop all I/O
            m_disconnecting = true;
            m_scanning = false;
            m_connected = false;
            // Disconnect signals to prevent further callbacks
            if (m_serial)
                m_serial->disconnect(this);
            // Defer actual cleanup to next event loop iteration
            QTimer::singleShot(0, this, &TinySADevice::disconnectDevice);
        }
    });

    m_connected = true;
    m_configReceived = false;
    m_buffer.clear();

    emit connectionChanged(true);

    // Probe for device version
    requestVersion();

    return true;
}

void TinySADevice::disconnectDevice()
{
    bool wasAlreadyDisconnecting = m_disconnecting;
    m_disconnecting = true;
    m_scanning = false;

    if (m_serial) {
        m_serial->disconnect(this);

        if (m_serial->isOpen() && !wasAlreadyDisconnecting) {
            m_serial->close();
        }
        m_serial->deleteLater();
        m_serial = nullptr;
    }

    bool wasConnected = m_connected;
    m_connected = false;
    m_configReceived = false;
    m_disconnecting = false;

    if (wasConnected)
        emit connectionChanged(false);
}

bool TinySADevice::isConnected() const
{
    return m_connected && m_serial && m_serial->isOpen();
}

bool TinySADevice::configure(double startFreqHz, double stopFreqHz, int sweepPoints)
{
    if (!isConnected())
        return false;

    m_scanning = false;
    m_sweepPoints = sweepPoints;

    // For Basic model, determine which frequency mode
    if (m_model == Model::Basic) {
        if (startFreqHz >= MIN_FREQ_BASIC_LOW && stopFreqHz <= MAX_FREQ_BASIC_LOW) {
            if (m_freqMode != FreqMode::Low) {
                m_freqMode = FreqMode::Low;
                sendTextCommand("mode low input");
                QThread::msleep(100);
            }
        } else if (startFreqHz >= MIN_FREQ_BASIC_HIGH && stopFreqHz <= MAX_FREQ_BASIC_HIGH) {
            if (m_freqMode != FreqMode::High) {
                m_freqMode = FreqMode::High;
                sendTextCommand("mode high input");
                QThread::msleep(100);
            }
        } else {
            emit errorOccurred(tr("Frequency range crosses TinySA Basic mode boundary"));
            return false;
        }
    }

    sendTextCommand(QString("sweep start %1").arg(static_cast<qint64>(startFreqHz)));
    QThread::msleep(50);
    sendTextCommand(QString("sweep stop %1").arg(static_cast<qint64>(stopFreqHz)));
    QThread::msleep(50);

    m_startFreqHz = startFreqHz;
    m_stopFreqHz = stopFreqHz;
    m_configReceived = true;

    emit deviceInfoUpdated();
    return true;
}

bool TinySADevice::startScanning()
{
    if (!isConnected() || !m_configReceived)
        return false;

    m_scanning = true;
    startScanRaw();
    return true;
}

void TinySADevice::stopScanning()
{
    m_scanning = false;
    m_scanDataStarted = false;
    m_parsedScanPoints = 0;
}

bool TinySADevice::isScanning() const
{
    return m_scanning;
}

void TinySADevice::sendTextCommand(const QString& cmd)
{
    if (!m_serial || !m_serial->isOpen() || m_serial->error() != QSerialPort::NoError)
        return;

    m_lastCommand = cmd;
    m_serial->write((cmd + "\r").toLatin1());
    m_serial->flush();
}

void TinySADevice::requestVersion()
{
    sendTextCommand("version");
}

void TinySADevice::requestSweepConfig()
{
    sendTextCommand("sweep");
}

void TinySADevice::startScanRaw()
{
    if (!isConnected() || !m_scanning)
        return;

    // Reset incremental scan state
    m_scanDataStarted = false;
    m_scanDataOffset = 0;
    m_parsedScanPoints = 0;

    sendTextCommand(QString("scanraw %1 %2 %3")
                        .arg(static_cast<qint64>(m_startFreqHz))
                        .arg(static_cast<qint64>(m_stopFreqHz))
                        .arg(m_sweepPoints));
}

void TinySADevice::onDataReady()
{
    if (!m_serial || m_disconnecting)
        return;

    m_buffer.append(m_serial->readAll());

    // Route scan data to incremental parser for real-time graphing
    if (m_lastCommand.startsWith("scanraw")) {
        processIncrementalScan();
        return;
    }

    processResponse();
}

void TinySADevice::processResponse()
{
    // TinySA responses are delimited by "ch> " prompt
    static const QByteArray delimiter("ch> ");

    while (true) {
        int promptIdx = m_buffer.indexOf(delimiter);
        if (promptIdx < 0)
            break;

        QByteArray response = m_buffer.left(promptIdx);
        m_buffer.remove(0, promptIdx + delimiter.size());

        if (response.isEmpty())
            continue;

        if (m_lastCommand == "version") {
            QStringList lines = QString::fromLatin1(response).split("\r\n", Qt::SkipEmptyParts);
            processVersionResponse(lines);
        } else if (m_lastCommand == "sweep") {
            QStringList lines = QString::fromLatin1(response).split("\r\n", Qt::SkipEmptyParts);
            processSweepConfigResponse(lines);
        } else if (m_lastCommand.startsWith("scanraw")) {
            processScanData(response);
        }
    }
}

void TinySADevice::processVersionResponse(const QStringList& lines)
{
    for (const auto& line : lines) {
        if (line.contains("tinySA4_")) {
            m_model = Model::Ultra;
            qDebug() << "TinySA model: Ultra";
        } else if (line.contains("tinySA_")) {
            m_model = Model::Basic;
            qDebug() << "TinySA model: Basic";
        } else if (line.contains("HW Version")) {
            m_hwVersion = line.section(':', 1).trimmed();
            qDebug() << "TinySA HW version:" << m_hwVersion;
        }
    }

    if (m_model != Model::Unknown) {
        emit deviceInfoUpdated();
        requestSweepConfig();
    }
}

void TinySADevice::processSweepConfigResponse(const QStringList& lines)
{
    // Response line format: "start stop steps"
    for (const auto& line : lines) {
        if (line == m_lastCommand)
            continue; // Skip command echo

        QStringList parts = line.trimmed().split(' ', Qt::SkipEmptyParts);
        if (parts.size() >= 2) {
            m_startFreqHz = parts[0].toDouble();
            m_stopFreqHz = parts[1].toDouble();
            m_configReceived = true;

            qDebug() << "TinySA config: Start:" << m_startFreqHz / 1e6 << "MHz"
                     << "Stop:" << m_stopFreqHz / 1e6 << "MHz";
            emit deviceInfoUpdated();
            break;
        }
    }
}

QVector<double> TinySADevice::parseScanAmplitudes(const char* data, int count) const
{
    QVector<double> amplitudes(count);
    double offsetVal = (m_model == Model::Basic) ? 128.0 : 174.0;

    for (int i = 0; i < count; ++i) {
        int offset = i * 3 + 1; // Skip first padding byte per triplet
        unsigned char lo = static_cast<unsigned char>(data[offset]);
        unsigned char hi = static_cast<unsigned char>(data[offset + 1]);
        int rawVal = lo + (hi * 256);
        amplitudes[i] = -((static_cast<double>(rawVal) / 32.0) - offsetVal);
    }

    return amplitudes;
}

void TinySADevice::processIncrementalScan()
{
    // Step 1: Find the opening '{' that marks the start of binary scan data
    if (!m_scanDataStarted) {
        int braceIdx = m_buffer.indexOf('{');
        if (braceIdx < 0)
            return; // Haven't received '{' yet
        m_scanDataOffset = braceIdx + 1;
        m_scanDataStarted = true;
        m_parsedScanPoints = 0;
    }

    // Step 2: How many complete 3-byte points are available?
    int availableBytes = m_buffer.size() - m_scanDataOffset;
    int availablePoints = std::min(availableBytes / 3, m_sweepPoints);
    int newPoints = availablePoints - m_parsedScanPoints;

    // Step 3: Emit partial update if enough new data arrived (but scan isn't complete)
    if (newPoints >= PARTIAL_SCAN_THRESHOLD && availablePoints < m_sweepPoints) {
        double freqStep = (m_sweepPoints > 1)
            ? (m_stopFreqHz - m_startFreqHz) / (m_sweepPoints - 1) : 0.0;

        QVector<double> amplitudes = parseScanAmplitudes(
            m_buffer.constData() + m_scanDataOffset, availablePoints);

        int pct = static_cast<int>(availablePoints * 100 / m_sweepPoints);
        emit sweepProgress(std::min(pct, 99));

        SweepData partial(m_startFreqHz, freqStep, amplitudes);
        emit partialSweepReady(partial);

        m_parsedScanPoints = availablePoints;
    }

    // Step 4: Check if the full scan has arrived
    int expectedDataBytes = m_sweepPoints * 3;
    if (availableBytes < expectedDataBytes)
        return; // Still waiting for more data

    // Wait for the 'ch> ' prompt so we can clean the buffer properly
    static const QByteArray delimiter("ch> ");
    int searchFrom = m_scanDataOffset + expectedDataBytes;
    int promptIdx = m_buffer.indexOf(delimiter, searchFrom);
    if (promptIdx < 0)
        return; // Data complete but prompt hasn't arrived yet

    // Parse the complete sweep
    double freqStep = (m_sweepPoints > 1)
        ? (m_stopFreqHz - m_startFreqHz) / (m_sweepPoints - 1) : 0.0;

    QVector<double> amplitudes = parseScanAmplitudes(
        m_buffer.constData() + m_scanDataOffset, m_sweepPoints);

    // Consume processed data from buffer
    m_buffer.remove(0, promptIdx + delimiter.size());

    // Reset incremental state
    m_scanDataStarted = false;
    m_parsedScanPoints = 0;

    // Emit completed sweep
    SweepData sweep(m_startFreqHz, freqStep, amplitudes);
    emit sweepProgress(100);
    emit sweepReady(sweep);

    // Request next scan if still scanning
    if (m_scanning)
        startScanRaw();
}

void TinySADevice::processScanData(const QByteArray& rawData)
{
    // Find the scan data block between { and }
    int startPos = rawData.indexOf('{');
    int stopPos = rawData.lastIndexOf('}');

    if (startPos < 0 || stopPos < 0 || stopPos <= startPos) {
        if (m_scanning)
            startScanRaw(); // Retry
        return;
    }

    QByteArray scanBytes = rawData.mid(startPos + 1, stopPos - startPos - 1);

    // Expected: 3 bytes per point (value byte, MSB, LSB — but actually just xML format)
    int expectedBytes = m_sweepPoints * 3;
    if (scanBytes.size() < expectedBytes) {
        qDebug() << "TinySA scan data too short:" << scanBytes.size() << "expected:" << expectedBytes;
        if (m_scanning)
            startScanRaw();
        return;
    }

    double freqStep = (m_sweepPoints > 1) ? (m_stopFreqHz - m_startFreqHz) / (m_sweepPoints - 1) : 0.0;
    QVector<double> amplitudes = parseScanAmplitudes(scanBytes.constData(), m_sweepPoints);

    SweepData sweep(m_startFreqHz, freqStep, amplitudes);
    emit sweepProgress(100);
    emit sweepReady(sweep);

    // Request next scan if still scanning
    if (m_scanning)
        startScanRaw();
}
