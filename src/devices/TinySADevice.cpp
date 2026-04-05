#include "TinySADevice.h"

#include <QDebug>
#include <QThread>
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
    m_scanning = false;

    if (m_serial) {
        if (m_serial->isOpen())
            m_serial->close();
        m_serial->deleteLater();
        m_serial = nullptr;
    }

    if (m_connected) {
        m_connected = false;
        m_configReceived = false;
        emit connectionChanged(false);
    }
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
}

bool TinySADevice::isScanning() const
{
    return m_scanning;
}

void TinySADevice::sendTextCommand(const QString& cmd)
{
    if (!m_serial || !m_serial->isOpen())
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

    sendTextCommand(QString("scanraw %1 %2 %3")
                        .arg(static_cast<qint64>(m_startFreqHz))
                        .arg(static_cast<qint64>(m_stopFreqHz))
                        .arg(m_sweepPoints));
}

void TinySADevice::onDataReady()
{
    if (!m_serial)
        return;

    m_buffer.append(m_serial->readAll());
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

    QVector<double> amplitudes(m_sweepPoints);
    double freqStep = (m_sweepPoints > 1) ? (m_stopFreqHz - m_startFreqHz) / (m_sweepPoints - 1) : 0.0;

    for (int i = 0; i < m_sweepPoints; ++i) {
        int offset = i * 3 + 1; // Skip first padding byte per triplet
        if (offset + 1 >= scanBytes.size())
            break;

        unsigned char lo = static_cast<unsigned char>(scanBytes[offset]);
        unsigned char hi = static_cast<unsigned char>(scanBytes[offset + 1]);
        int rawVal = lo + (hi * 256);

        // Convert to dBm
        double offsetVal = (m_model == Model::Basic) ? 128.0 : 174.0;
        amplitudes[i] = -((static_cast<double>(rawVal) / 32.0) - offsetVal);
    }

    SweepData sweep(m_startFreqHz, freqStep, amplitudes);
    emit sweepReady(sweep);

    // Request next scan if still scanning
    if (m_scanning)
        startScanRaw();
}
