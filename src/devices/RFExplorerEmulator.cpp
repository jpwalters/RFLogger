#include "RFExplorerEmulator.h"

#include <QDebug>
#include <QRandomGenerator>
#include <cmath>
#include <cstring>

RFExplorerEmulator::RFExplorerEmulator(const Model& model, QObject* parent)
    : QIODevice(parent)
    , m_model(model)
{
    m_startKHz = m_model.minFreqKHz;
    m_stopKHz = m_model.minFreqKHz + 20000; // default 20 MHz span
    m_timer = new QTimer(this);
    m_timer->setInterval(5);
    connect(m_timer, &QTimer::timeout, this, &RFExplorerEmulator::sendSweep);
}

bool RFExplorerEmulator::open(OpenMode mode)
{
    return QIODevice::open(mode);
}

void RFExplorerEmulator::setSweepInterval(int ms)
{
    if (ms < 1)
        ms = 1;
    m_timer->setInterval(ms);
}

qint64 RFExplorerEmulator::bytesAvailable() const
{
    return m_outbound.size() + QIODevice::bytesAvailable();
}

qint64 RFExplorerEmulator::readData(char* data, qint64 maxSize)
{
    const qint64 n = qMin<qint64>(maxSize, m_outbound.size());
    if (n > 0) {
        std::memcpy(data, m_outbound.constData(), static_cast<size_t>(n));
        m_outbound.remove(0, static_cast<int>(n));
    }
    return n;
}

qint64 RFExplorerEmulator::writeData(const char* data, qint64 maxSize)
{
    m_inbound.append(data, static_cast<int>(maxSize));

    // RF Explorer host commands are '#' + length_byte + payload, where the
    // length byte counts '#' and itself. Parse all complete commands.
    while (!m_inbound.isEmpty()) {
        if (static_cast<unsigned char>(m_inbound[0]) != '#') {
            m_inbound.remove(0, 1); // resync
            continue;
        }
        if (m_inbound.size() < 2)
            break;
        const int len = static_cast<unsigned char>(m_inbound[1]);
        if (len < 2 || m_inbound.size() < len)
            break;
        const QByteArray cmd = m_inbound.left(len);
        m_inbound.remove(0, len);
        handleCommand(cmd);
    }

    return maxSize;
}

void RFExplorerEmulator::handleCommand(const QByteArray& cmd)
{
    // cmd[0]='#', cmd[1]=len, cmd[2..] payload
    const QByteArray payload = cmd.mid(2);

    if (payload.startsWith("C0")) {
        // GET_CONFIG — device replies with model + config and resumes streaming
        qDebug() << "[emu] GET_CONFIG";
        sendModelData();
        sendConfigData();
        startStreaming();
    }
    else if (payload.startsWith("CH")) {
        qDebug() << "[emu] HOLD";
        stopStreaming();
    }
    else if (payload.startsWith("CJ")) {
        // SET_SWEEP_POINTS (basic): points = (byte + 1) * 16
        const int encoded = static_cast<unsigned char>(payload[2]);
        m_lastRequestedPoints = (encoded + 1) * 16;
        qDebug() << "[emu] SET_SWEEP_POINTS (basic) requested" << m_lastRequestedPoints;
    }
    else if (payload.startsWith("Cj")) {
        // SET_SWEEP_POINTS_LARGE (PLUS): points = MSB<<8 | LSB
        const int msb = static_cast<unsigned char>(payload[2]);
        const int lsb = static_cast<unsigned char>(payload[3]);
        m_lastRequestedPoints = (msb << 8) | lsb;
        qDebug() << "[emu] SET_SWEEP_POINTS_LARGE (plus) requested" << m_lastRequestedPoints;
    }
    else if (payload.startsWith("C2-F:")) {
        // SET_CONFIG: "C2-F:<start7>,<stop7>,<top4>,<bottom4>"
        const QByteArray fields = payload.mid(5);
        const QList<QByteArray> parts = fields.split(',');
        if (parts.size() >= 2) {
            bool ok1 = false, ok2 = false;
            const int startKHz = parts[0].toInt(&ok1);
            const int stopKHz = parts[1].toInt(&ok2);
            if (ok1 && ok2 && startKHz < stopKHz) {
                m_startKHz = startKHz;
                m_stopKHz = stopKHz;
            }
        }

        // Apply requested points, clamped/snapped to the device's real limits.
        int pts = m_lastRequestedPoints;
        if (pts > m_model.maxDevicePoints)
            pts = m_model.maxDevicePoints;
        if (pts < 16)
            pts = 16;
        if (!m_model.plus())
            pts = (pts / 16) * 16; // basic snaps to multiple of 16
        m_points = pts;

        qDebug() << "[emu] SET_CONFIG" << m_startKHz << "-" << m_stopKHz << "kHz, points ->" << m_points;
        sendConfigData();
        startStreaming();
    }
    else {
        qDebug() << "[emu] unknown cmd" << cmd.toHex(' ');
    }
}

void RFExplorerEmulator::sendModelData()
{
    const QByteArray msg = QByteArray("#C2-M:")
        + QByteArray::number(m_model.mainCode) + ','
        + QByteArray::number(m_model.expansionCode) + ','
        + m_model.firmware.toLatin1()
        + "\r\n";
    queue(msg);
}

void RFExplorerEmulator::sendConfigData()
{
    // Format expected by RFExplorerDevice::processConfigData():
    // startKHz,stepHz,ampTop,ampBottom,points,expActive,mode,minKHz,maxKHz,maxSpanKHz
    const double spanHz = (m_stopKHz - m_startKHz) * 1000.0;
    const double stepHz = (m_points > 1) ? spanHz / (m_points - 1) : 0.0;

    QByteArray msg = "#C2-F:";
    msg += QByteArray::number(m_startKHz) + ',';
    msg += QByteArray::number(stepHz, 'f', 0) + ',';
    msg += "-010,";   // amp top dBm
    msg += "-120,";   // amp bottom dBm
    msg += QByteArray::number(m_points) + ',';
    msg += "0,";      // expansion module active
    msg += "0,";      // current mode
    msg += QByteArray::number(m_model.minFreqKHz) + ',';
    msg += QByteArray::number(m_model.maxFreqKHz) + ',';
    msg += QByteArray::number(m_model.maxSpanKHz);
    msg += "\r\n";
    queue(msg);
}

void RFExplorerEmulator::startStreaming()
{
    if (!m_timer->isActive())
        m_timer->start();
}

void RFExplorerEmulator::stopStreaming()
{
    m_timer->stop();
}

void RFExplorerEmulator::sendSweep()
{
    // Build one sweep covering the full configured span. Synthesise a realistic
    // looking spectrum: a gently wandering noise floor near -100 dBm with a few
    // stable carriers so the live plot and waterfall visibly show activity.
    // Amplitude byte encodes dBm as raw = dBm * -2 (raw 200 = -100 dBm).
    QByteArray amplitudes(m_points, static_cast<char>(200));
    auto* rng = QRandomGenerator::global();

    auto setDbm = [&](int idx, double dbm) {
        if (idx < 0 || idx >= m_points)
            return;
        int raw = static_cast<int>(std::lround(dbm * -2.0));
        raw = std::clamp(raw, 0, 255);
        amplitudes[idx] = static_cast<char>(raw);
    };

    // Wandering noise floor: -102..-96 dBm.
    for (int i = 0; i < m_points; ++i) {
        double noise = -99.0 + (rng->bounded(60) - 30) / 10.0;
        setDbm(i, noise);
    }

    // A handful of carriers at fixed fractions of the span, each with a small
    // Gaussian-ish skirt so they render as peaks rather than single spikes.
    struct Carrier { double frac; double peakDbm; int width; };
    static const Carrier carriers[] = {
        {0.15, -35.0, 6},
        {0.38, -22.0, 9},
        {0.61, -48.0, 4},
        {0.80, -30.0, 7},
    };
    for (const auto& c : carriers) {
        const int center = static_cast<int>(c.frac * (m_points - 1));
        const double jitter = (rng->bounded(40) - 20) / 10.0; // +/-2 dB
        for (int d = -c.width; d <= c.width; ++d) {
            const double falloff = 1.0 - static_cast<double>(std::abs(d)) / (c.width + 1);
            const double dbm = -99.0 + (c.peakDbm + jitter + 99.0) * falloff;
            setDbm(center + d, dbm);
        }
    }

    QByteArray msg;
    if (m_model.plus()) {
        // SCAN_DATA_LARGE: '$z' + MSB + LSB + data + \r\n
        msg += "$z";
        msg += static_cast<char>((m_points >> 8) & 0xFF);
        msg += static_cast<char>(m_points & 0xFF);
    } else {
        // SCAN_DATA_EXT (high-res): '$s' + encoded + data + \r\n, points=(enc+1)*16
        msg += "$s";
        msg += static_cast<char>((m_points / 16) - 1);
    }
    msg += amplitudes;
    msg += "\r\n";
    queue(msg);
    ++m_sweepsSent;
}

void RFExplorerEmulator::queue(const QByteArray& bytes)
{
    m_outbound.append(bytes);
    scheduleReadyRead();
}

void RFExplorerEmulator::scheduleReadyRead()
{
    if (m_emitScheduled)
        return;
    m_emitScheduled = true;
    QTimer::singleShot(0, this, [this]() {
        m_emitScheduled = false;
        if (!m_outbound.isEmpty())
            emit readyRead();
    });
}
