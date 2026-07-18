#include "RFExplorerDevice.h"

#include <QDebug>
#include <QThread>
#include <cmath>

// RF Explorer UART API: https://github.com/RFExplorer/RFExplorer-for-.NET/wiki/RF-Explorer-UART-API-interface-specification
// Protocol reference: berkon/wireless-microphone-analyzer scan_devices/rf_explorer.js

RFExplorerDevice::RFExplorerDevice(QObject* parent)
    : ISpectrumDevice(parent)
{
}

RFExplorerDevice::~RFExplorerDevice()
{
    disconnectDevice();
}

QString RFExplorerDevice::deviceName() const
{
    if (m_isPlusModel)
        return QStringLiteral("RF Explorer PLUS");
    return QStringLiteral("RF Explorer");
}

QString RFExplorerDevice::modelName() const
{
    auto modelString = [](int code) -> QString {
        switch (code) {
        case 0:  return "433M";
        case 1:  return "868M";
        case 2:  return "915M";
        case 3:  return "WSUB1G";
        case 4:  return "2.4G";
        case 5:  return "WSUB3G";
        case 6:  return "6G";
        case 10: return "WSUB1G_PLUS";
        case 12: return "2.4G_PLUS";
        case 13: return "4G_PLUS";
        case 14: return "6G_PLUS";
        default: return "Unknown";
        }
    };

    QString name = modelString(m_mainModelCode);
    if (m_expansionModelCode >= 0 && m_expansionModelCode != 255)
        name += " + " + modelString(m_expansionModelCode);
    return name;
}

int RFExplorerDevice::minSweepPoints() const
{
    return m_isPlusModel ? MIN_SWEEP_POINTS_PLUS : MIN_SWEEP_POINTS_BASIC;
}

int RFExplorerDevice::maxSweepPoints() const
{
    return m_isPlusModel ? MAX_SWEEP_POINTS_PLUS : MAX_SWEEP_POINTS_BASIC;
}

bool RFExplorerDevice::connectDevice(const QString& portName)
{
    if (m_connected)
        disconnectDevice();

    auto* serial = new QSerialPort(this);
    serial->setPortName(portName);
    serial->setBaudRate(BAUD_RATE);
    serial->setDataBits(QSerialPort::Data8);
    serial->setParity(QSerialPort::NoParity);
    serial->setStopBits(QSerialPort::OneStop);
    serial->setFlowControl(QSerialPort::NoFlowControl);

    if (!serial->open(QIODevice::ReadWrite)) {
        emit errorOccurred(tr("Failed to open %1: %2").arg(portName, serial->errorString()));
        delete serial;
        return false;
    }

    return attachTransport(serial);
}

bool RFExplorerDevice::connectTransport(QIODevice* transport)
{
    if (m_connected)
        disconnectDevice();

    if (!transport)
        return false;

    if (!transport->isOpen() && !transport->open(QIODevice::ReadWrite)) {
        emit errorOccurred(tr("Failed to open transport"));
        return false;
    }

    return attachTransport(transport);
}

bool RFExplorerDevice::attachTransport(QIODevice* transport)
{
    m_serial = transport;
    m_serial->setParent(this);

    connect(m_serial, &QIODevice::readyRead, this, &RFExplorerDevice::onDataReady);

    // Serial-port-specific removal handling (physical unplug). In-memory
    // transports (e.g. the test emulator) do not emit this signal.
    if (auto* sp = qobject_cast<QSerialPort*>(m_serial)) {
        connect(sp, &QSerialPort::errorOccurred, this, [this](QSerialPort::SerialPortError error) {
            if (error == QSerialPort::ResourceError && !m_disconnecting) {
                qDebug() << "RF Explorer: device removed (ResourceError)";
                // Mark as disconnecting immediately to stop all I/O
                m_disconnecting = true;
                m_scanning = false;
                m_connected = false;
                // Disconnect signals to prevent further callbacks
                if (m_serial)
                    m_serial->disconnect(this);
                // Defer actual cleanup to next event loop iteration —
                // calling close()/deleteLater() inside errorOccurred crashes
                QTimer::singleShot(0, this, &RFExplorerDevice::disconnectDevice);
            }
        });
    }

    m_connected = true;
    m_configReceived = false;
    m_buffer.clear();

    emit connectionChanged(true);

    // Request device configuration — this also triggers the device to start sending data
    requestConfig();

    return true;
}

void RFExplorerDevice::disconnectDevice()
{
    // Allow re-entry from the deferred QTimer::singleShot after ResourceError,
    // but prevent true recursive re-entrancy within the same call.
    bool wasAlreadyDisconnecting = m_disconnecting;
    m_disconnecting = true;
    m_scanning = false;

    if (m_serial) {
        // Disconnect all signals first to prevent re-entrancy
        m_serial->disconnect(this);

        if (m_serial->isOpen()) {
            // Only send HOLD command if the port is still healthy.
            // If the device was physically unplugged, writing will crash.
            if (!transportHasError() && !wasAlreadyDisconnecting) {
                QByteArray holdCmd = QByteArray("#\x04" "CH", 4);
                m_serial->write(holdCmd);
                flushTransport();
                QThread::msleep(50);
            }
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

bool RFExplorerDevice::isConnected() const
{
    return m_connected && m_serial && m_serial->isOpen();
}

bool RFExplorerDevice::requestConfig()
{
    if (!isConnected())
        return false;

    // GET_CONFIG command: '#' + length_byte(0x04) + 'C0'
    QByteArray cmd(4, '\0');
    cmd[0] = '#';
    cmd[1] = 0x04;
    cmd[2] = 'C';
    cmd[3] = '0';
    sendCommand(cmd);
    return true;
}

bool RFExplorerDevice::configure(double startFreqHz, double stopFreqHz, int sweepPoints)
{
    if (!isConnected())
        return false;

    if (startFreqHz < m_minFreqHz || stopFreqHz > m_maxFreqHz || startFreqHz >= stopFreqHz) {
        emit errorOccurred(tr("Frequency range %1-%2 MHz out of device bounds (%3-%4 MHz)")
                               .arg(startFreqHz / 1e6, 0, 'f', 3)
                               .arg(stopFreqHz / 1e6, 0, 'f', 3)
                               .arg(m_minFreqHz / 1e6, 0, 'f', 3)
                               .arg(m_maxFreqHz / 1e6, 0, 'f', 3));
        return false;
    }

    m_desiredStartHz = startFreqHz;
    m_desiredStopHz = stopFreqHz;
    m_desiredSweepPoints = sweepPoints;

    // Send HOLD to stop the device data dump, then clear our
    // application-level buffer and accumulator so the parser
    // starts cleanly with the new configuration.
    QByteArray holdCmd(4, '\0');
    holdCmd[0] = '#';
    holdCmd[1] = 0x04;
    holdCmd[2] = 'C';
    holdCmd[3] = 'H';
    sendCommand(holdCmd);

    m_buffer.clear();
    m_accumBuffer.clear();
    // The first sweep after reconfiguration often carries stale amplitude
    // data from the device's internal buffer.  Discard it.
    m_discardCount = 1;

    // Compute the actual sweep points the device will use (basic models
    // snap to multiples of 16).
    int actualPoints = sweepPoints;

    // Set sweep points BEFORE frequency config so the device applies both
    // before it starts the new sweep stream.
    {
        if (m_isPlusModel) {
            // PLUS models render high resolution on the device. Send the full
            // requested point count, clamped only to the documented hardware
            // ceiling (MAX_SWEEP_POINTS_PLUS = 65535). Clamping lower than this
            // would throw away resolution the device can actually deliver.
            int clamped = sweepPoints;
            if (clamped > MAX_SWEEP_POINTS_PLUS) clamped = MAX_SWEEP_POINTS_PLUS;
            if (clamped < MIN_SWEEP_POINTS_PLUS) clamped = MIN_SWEEP_POINTS_PLUS;
            actualPoints = clamped;
            // SET_SWEEP_POINTS_LARGE: '#' + 0x06 + 'Cj' + MSB + LSB
            QByteArray sweepCmd(6, '\0');
            sweepCmd[0] = '#';
            sweepCmd[1] = 0x06;
            sweepCmd[2] = 'C';
            sweepCmd[3] = 'j';
            sweepCmd[4] = static_cast<char>((clamped >> 8) & 0xFF);
            sweepCmd[5] = static_cast<char>(clamped & 0xFF);
            sendCommand(sweepCmd);
        } else {
            // SET_SWEEP_POINTS (high-res): '#' + 0x05 + 'CJ' + encoded_byte
            // Snap to multiple of 16; encoded value = (points / 16) - 1
            int snapped = (sweepPoints / 16) * 16;
            if (snapped < 16) snapped = 16;
            if (snapped > MAX_SWEEP_POINTS_BASIC) snapped = MAX_SWEEP_POINTS_BASIC;
            actualPoints = snapped;
            QByteArray sweepCmd(5, '\0');
            sweepCmd[0] = '#';
            sweepCmd[1] = 0x05;
            sweepCmd[2] = 'C';
            sweepCmd[3] = 'J';
            sweepCmd[4] = static_cast<char>((snapped / 16) - 1);
            sendCommand(sweepCmd);
        }
    }

    // SET_CONFIG: '#' + 0x20 + 'C2-F:' + start_khz(7 digits) + ',' + stop_khz(7 digits) + ',top_dBm(4 chars),bottom_dBm(4 chars)'
    // start/stop in kHz, 7-digit zero-padded; amplitude fields are 4 ASCII chars each (e.g. "-010","-120")
    int startKhz = static_cast<int>(std::floor(startFreqHz / 1000.0));
    int stopKhz = static_cast<int>(std::floor(stopFreqHz / 1000.0));

    QString startStr = QString("%1").arg(startKhz, 7, 10, QChar('0'));
    QString stopStr = QString("%1").arg(stopKhz, 7, 10, QChar('0'));

    // Amplitude range: top dBm and bottom dBm (4 chars each per UART API spec)
    QString configPayload = QString("C2-F:%1,%2,-010,-120").arg(startStr, stopStr);
    QByteArray cmd;
    cmd.append('#');
    cmd.append(static_cast<char>(configPayload.length() + 2)); // +2 for '#' and length byte = 32
    cmd.append(configPayload.toLatin1());
    sendCommand(cmd);

    // Apply the requested config locally so scan data is plotted with the
    // correct frequency range immediately — without waiting for the device's
    // config echo (which can occasionally be lost or delayed).
    m_startFreqHz = startKhz * 1000.0;  // kHz resolution, matching what we sent
    m_sweepPoints = actualPoints;
    m_freqStepHz = (actualPoints > 1)
        ? (stopKhz * 1000.0 - startKhz * 1000.0) / (actualPoints - 1)
        : 0.0;

    // Still request the config echo so processConfigData() can refine our
    // values with the device's actual rounding, but do NOT gate scan
    // processing on it.
    requestConfig();

    return true;
}

bool RFExplorerDevice::startScanning()
{
    if (!isConnected())
        return false;

    m_scanning = true;

    // Reset sub-sweep accumulator
    m_accumBuffer.clear();
    m_accumStartHz = 0.0;
    m_accumStepHz = 0.0;
    m_accumTarget = m_desiredSweepPoints;

    if (!m_configReceived)
        requestConfig();
    return true;
}

void RFExplorerDevice::stopScanning()
{
    m_scanning = false;

    if (!isConnected())
        return;

    // Send HOLD command
    QByteArray holdCmd(4, '\0');
    holdCmd[0] = '#';
    holdCmd[1] = 0x04;
    holdCmd[2] = 'C';
    holdCmd[3] = 'H';
    sendCommand(holdCmd);
}

bool RFExplorerDevice::isScanning() const
{
    return m_scanning;
}

void RFExplorerDevice::sendCommand(const QByteArray& cmd)
{
    if (!m_serial || !m_serial->isOpen() || transportHasError())
        return;

    m_serial->write(cmd);
    flushTransport();
}

void RFExplorerDevice::flushTransport()
{
    if (auto* sp = qobject_cast<QSerialPort*>(m_serial))
        sp->flush();
}

bool RFExplorerDevice::transportHasError() const
{
    if (auto* sp = qobject_cast<QSerialPort*>(m_serial))
        return sp->error() != QSerialPort::NoError;
    return false;
}

void RFExplorerDevice::onDataReady()
{
    if (!m_serial || m_disconnecting)
        return;

    m_buffer.append(m_serial->readAll());
    
    // Defer buffer processing to avoid blocking the event loop with large amounts of incoming data.
    // This allows the UI to remain responsive during active scanning.
    if (!m_processBufferScheduled) {
        m_processBufferScheduled = true;
        QTimer::singleShot(0, this, &RFExplorerDevice::processDeferredBuffer);
    }
}

void RFExplorerDevice::processDeferredBuffer()
{
    m_processBufferScheduled = false;
    
    // Process available data. If more arrives while processing, schedule another round.
    processBuffer(true);
}

void RFExplorerDevice::stripEosSequences(QByteArray& buffer)
{
    // PLUS models send EOS: FF FE FF FE 00 between messages.
    // Strip all occurrences. Since amplitude byte 0x00 (0 dBm) is extremely
    // rare with the preceding FF FE FF FE pattern, false positives are
    // negligible, and the positional scan data parsing recovers gracefully.
    static const QByteArray eos("\xFF\xFE\xFF\xFE\x00", 5);
    int idx;
    while ((idx = buffer.indexOf(eos)) != -1) {
        buffer.remove(idx, 5);
    }
}

void RFExplorerDevice::processBuffer(bool isDeferred)
{
    stripEosSequences(m_buffer);

    // Limit messages processed per call to avoid blocking the event loop.
    // If more messages remain, schedule deferred processing to allow UI updates.
    static const int MESSAGES_PER_BATCH = 50;
    int messagesProcessed = 0;

    while (messagesProcessed < MESSAGES_PER_BATCH) {
        // Look for message delimiters: \r\n terminated messages
        // Messages starting with '#' are config/info, '$' are scan data

        int hashIdx = m_buffer.indexOf('#');
        int dollarIdx = m_buffer.indexOf('$');

        // Find the earliest message start
        int msgStart = -1;
        if (hashIdx >= 0 && dollarIdx >= 0)
            msgStart = std::min(hashIdx, dollarIdx);
        else if (hashIdx >= 0)
            msgStart = hashIdx;
        else if (dollarIdx >= 0)
            msgStart = dollarIdx;
        else
            break;

        // Discard any data before the message start
        if (msgStart > 0)
            m_buffer.remove(0, msgStart);

        // Check what kind of message we have
        if (m_buffer.startsWith("#C2-F:")) {
            // CONFIG_DATA — terminated by \r\n
            int endIdx = m_buffer.indexOf("\r\n", 6);
            if (endIdx < 0)
                break; // Wait for more data
            QByteArray configData = m_buffer.mid(6, endIdx - 6);
            m_buffer.remove(0, endIdx + 2);
            processConfigData(configData);
            ++messagesProcessed;
        }
        else if (m_buffer.startsWith("#C2-M:")) {
            // MODEL_DATA — terminated by \r\n
            int endIdx = m_buffer.indexOf("\r\n", 6);
            if (endIdx < 0)
                break;
            QByteArray modelData = m_buffer.mid(6, endIdx - 6);
            m_buffer.remove(0, endIdx + 2);
            processModelData(modelData);
            ++messagesProcessed;
        }
        else if (m_buffer.startsWith("#Sn")) {
            // SERIAL_NUMBER — terminated by \r\n
            int endIdx = m_buffer.indexOf("\r\n", 3);
            if (endIdx < 0)
                break;
            m_serialNumber = QString::fromLatin1(m_buffer.mid(3, endIdx - 3));
            m_buffer.remove(0, endIdx + 2);
            qDebug() << "RF Explorer serial:" << m_serialNumber;
            emit deviceInfoUpdated();
            ++messagesProcessed;
        }
        else if (m_buffer.startsWith("$S")) {
            // SCAN_DATA (standard) — $S + 1 byte count + <count> amplitude bytes + \r\n
            if (m_buffer.size() < 3)
                break;
            int sweepPoints = static_cast<unsigned char>(m_buffer[2]);
            int totalLen = 3 + sweepPoints + 2; // $S + count_byte + data + \r\n
            if (m_buffer.size() < totalLen)
                break;
            QByteArray scanData = m_buffer.mid(3, sweepPoints);
            m_buffer.remove(0, totalLen);
            if (m_configReceived && m_scanning)
                processScanData(scanData, sweepPoints);
            ++messagesProcessed;
        }
        else if (m_buffer.startsWith("$s")) {
            // SCAN_DATA_EXT (high-res) — $s + 1 byte encoded count + <decoded_count> amplitude bytes + \r\n
            // Actual points = (encoded_byte + 1) * 16
            if (m_buffer.size() < 3)
                break;
            int encoded = static_cast<unsigned char>(m_buffer[2]);
            int sweepPoints = (encoded + 1) * 16;
            int totalLen = 3 + sweepPoints + 2; // $s + encoded_byte + data + \r\n
            if (m_buffer.size() < totalLen)
                break;
            QByteArray scanData = m_buffer.mid(3, sweepPoints);
            m_buffer.remove(0, totalLen);
            if (m_configReceived && m_scanning)
                processScanData(scanData, sweepPoints);
            ++messagesProcessed;
        }
        else if (m_buffer.startsWith("$z")) {
            // SCAN_DATA_LARGE (PLUS) — $z + 2 bytes MSB/LSB count + <count> amplitude bytes + \r\n
            if (m_buffer.size() < 4)
                break;
            int msb = static_cast<unsigned char>(m_buffer[2]);
            int lsb = static_cast<unsigned char>(m_buffer[3]);
            int sweepPoints = (msb << 8) | lsb;
            int totalLen = 4 + sweepPoints + 2; // $z + 2 count bytes + data + \r\n
            if (m_buffer.size() < totalLen)
                break;
            QByteArray scanData = m_buffer.mid(4, sweepPoints);
            m_buffer.remove(0, totalLen);
            if (m_configReceived && m_scanning)
                processScanData(scanData, sweepPoints);
            ++messagesProcessed;
        }
        else if (m_buffer.startsWith("#")) {
            // Other # messages — find \r\n and skip
            int endIdx = m_buffer.indexOf("\r\n", 1);
            if (endIdx < 0)
                break;
            QByteArray msg = m_buffer.mid(0, endIdx);
            m_buffer.remove(0, endIdx + 2);
            qDebug() << "RF Explorer msg:" << msg.toHex(' ');
            ++messagesProcessed;
        }
        else {
            // Unknown data starting with $ but not $S or $z
            // Skip to next \r\n
            int endIdx = m_buffer.indexOf("\r\n");
            if (endIdx < 0)
                break;
            m_buffer.remove(0, endIdx + 2);
        }
    }

    // If we hit the message limit and still have data to process,
    // schedule deferred processing to keep the event loop responsive.
    if (messagesProcessed >= MESSAGES_PER_BATCH && !m_buffer.isEmpty() && !m_processBufferScheduled) {
        m_processBufferScheduled = true;
        QTimer::singleShot(0, this, &RFExplorerDevice::processDeferredBuffer);
    }
}

void RFExplorerDevice::processConfigData(const QByteArray& data)
{
    // Format: startFreqKHz,freqStepHz,ampTopDBm,ampBottomDBm,sweepPoints,expModActive,currentMode,minFreqKHz,maxFreqKHz,maxSpanKHz,...
    QList<QByteArray> parts = data.split(',');
    if (parts.size() < 10) {
        qWarning() << "RF Explorer config data too short:" << data;
        return;
    }

    bool ok = false;

    double startFreqKhz = parts[0].toDouble(&ok);
    if (!ok) { qWarning() << "RF Explorer: invalid startFreqKhz:" << parts[0]; return; }

    double freqStepHz = parts[1].toDouble(&ok);
    if (!ok) { qWarning() << "RF Explorer: invalid freqStepHz:" << parts[1]; return; }

    int sweepPoints = parts[4].toInt(&ok);
    if (!ok || sweepPoints <= 0) {
        qWarning() << "RF Explorer: invalid sweepPoints:" << parts[4];
        return;
    }

    double minFreqKhz = parts[7].toDouble(&ok);
    if (!ok) { qWarning() << "RF Explorer: invalid minFreqKhz:" << parts[7]; return; }

    double maxFreqKhz = parts[8].toDouble(&ok);
    if (!ok) { qWarning() << "RF Explorer: invalid maxFreqKhz:" << parts[8]; return; }

    double maxSpanKhz = parts[9].toDouble(&ok);
    if (!ok) { qWarning() << "RF Explorer: invalid maxSpanKhz:" << parts[9]; return; }

    m_startFreqHz = startFreqKhz * 1000.0;
    m_freqStepHz = freqStepHz;
    m_sweepPoints = sweepPoints;
    m_minFreqHz = minFreqKhz * 1000.0;
    m_maxFreqHz = maxFreqKhz * 1000.0;
    m_maxSpanHz = maxSpanKhz * 1000.0;
    m_configReceived = true;

    double stopFreqHz = m_startFreqHz + m_freqStepHz * (m_sweepPoints - 1);

    qDebug() << "RF Explorer config:"
             << "Start:" << m_startFreqHz / 1e6 << "MHz"
             << "Stop:" << stopFreqHz / 1e6 << "MHz"
             << "Step:" << m_freqStepHz << "Hz"
             << "Points:" << m_sweepPoints
             << "Range:" << m_minFreqHz / 1e6 << "-" << m_maxFreqHz / 1e6 << "MHz";

    emit deviceInfoUpdated();
}

void RFExplorerDevice::processScanData(const QByteArray& data, int sweepPoints)
{
    // Discard the first sweep after reconfiguration — the device often sends
    // stale amplitude data from its internal buffer.
    if (m_discardCount > 0) {
        --m_discardCount;
        return;
    }

    QVector<double> amplitudes(sweepPoints);

    for (int i = 0; i < sweepPoints && i < static_cast<int>(data.size()); ++i) {
        // Each byte is an amplitude value: dBm = byte_value / -2.0
        // So 0 = 0 dBm, 1 = -0.5 dBm, 240 = -120 dBm
        unsigned char raw = static_cast<unsigned char>(data[i]);
        amplitudes[i] = static_cast<double>(raw) / AMPLITUDE_DIVISOR;
    }

    // If the device delivers at least the requested point count in one message,
    // emit directly without accumulation overhead.
    if (sweepPoints >= m_accumTarget) {
        emit sweepProgress(100);
        SweepData sweep(m_startFreqHz, m_freqStepHz, amplitudes);
        emit sweepReady(sweep);
        return;
    }

    // Sub-sweep accumulation: the device sends the full range in multiple
    // smaller sweeps.  Append each chunk and emit once the target is reached.
    // The accumulated result is labelled with the requested (desired) frequency
    // range so the stitched sweep always spans exactly what the user asked for.
    // Using the single-sweep step here instead would stretch the frequency axis
    // to several times the requested span (the "thin green line" regression).
    if (m_accumBuffer.isEmpty()) {
        m_accumStartHz = m_startFreqHz;
        m_accumStepHz = m_freqStepHz;
    }

    m_accumBuffer.append(amplitudes);

    int pct = static_cast<int>(m_accumBuffer.size()) * 100 / m_accumTarget;
    emit sweepProgress(std::min(pct, 99));

    // Emit partial sweep so the UI can graph data as it arrives.
    double partialStepHz = (m_accumBuffer.size() > 1)
        ? (m_desiredStopHz - m_desiredStartHz) / (m_accumTarget - 1)
        : m_accumStepHz;
    SweepData partial(m_desiredStartHz, partialStepHz, m_accumBuffer);
    emit partialSweepReady(partial);

    if (static_cast<int>(m_accumBuffer.size()) >= m_accumTarget) {
        // Trim to exact target in case the device overshot slightly
        m_accumBuffer.resize(m_accumTarget);

        // Recalculate step size for the full accumulated range.
        double fullStepHz = (m_accumTarget > 1)
            ? (m_desiredStopHz - m_desiredStartHz) / (m_accumTarget - 1)
            : 0.0;

        emit sweepProgress(100);
        SweepData sweep(m_desiredStartHz, fullStepHz, m_accumBuffer);
        emit sweepReady(sweep);
        m_accumBuffer.clear();
    }
}

void RFExplorerDevice::processModelData(const QByteArray& data)
{
    QList<QByteArray> parts = data.split(',');
    if (parts.size() < 3) {
        qWarning() << "RF Explorer model data too short:" << data;
        return;
    }

    m_mainModelCode = parts[0].toInt();
    m_expansionModelCode = parts[1].toInt();
    m_firmwareVersion = QString::fromLatin1(parts[2]);

    // PLUS models have model codes >= 10
    m_isPlusModel = (m_mainModelCode >= 10);

    qDebug() << "RF Explorer model:" << modelName()
             << "FW:" << m_firmwareVersion
             << "PLUS:" << m_isPlusModel;

    emit deviceInfoUpdated();
}
