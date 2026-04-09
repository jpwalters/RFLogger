#include "TvChannelMap.h"

QVector<TvChannel> TvChannelMap::channelsInRange(double startHz, double stopHz) const
{
    QVector<TvChannel> result;
    result.reserve(m_channels.size());
    for (const auto& ch : m_channels) {
        if (ch.stopFreqHz > startHz && ch.startFreqHz < stopHz)
            result.append(ch);
    }
    return result;
}

const TvChannel* TvChannelMap::channelAtFrequency(double freqHz) const
{
    for (const auto& ch : m_channels) {
        if (freqHz >= ch.startFreqHz && freqHz < ch.stopFreqHz)
            return &ch;
    }
    return nullptr;
}

QStringList TvChannelMap::availableRegions()
{
    return { "US (ATSC)", "Europe (DVB-T)", "Japan (ISDB-T)" };
}

TvChannelMap TvChannelMap::forRegion(const QString& region)
{
    if (region == "Europe (DVB-T)")
        return euDvbt();
    if (region == "Japan (ISDB-T)")
        return jpIsdb();
    return usAtsc();   // default
}

TvChannelMap TvChannelMap::usAtsc()
{
    TvChannelMap map;
    constexpr double MHz = 1'000'000.0;

    // VHF-Lo: Channels 2–6 (54–88 MHz, 6 MHz each)
    double startMHz = 54.0;
    for (int ch = 2; ch <= 6; ++ch) {
        map.m_channels.append({ch,
                               QString("Ch %1").arg(ch),
                               startMHz * MHz,
                               (startMHz + 6.0) * MHz,
                               "VHF-Lo"});
        startMHz += 6.0;
    }

    // VHF-Hi: Channels 7–13 (174–216 MHz, 6 MHz each)
    startMHz = 174.0;
    for (int ch = 7; ch <= 13; ++ch) {
        map.m_channels.append({ch,
                               QString("Ch %1").arg(ch),
                               startMHz * MHz,
                               (startMHz + 6.0) * MHz,
                               "VHF-Hi"});
        startMHz += 6.0;
    }

    // UHF: Channels 14–36 (470–608 MHz, 6 MHz each)
    startMHz = 470.0;
    for (int ch = 14; ch <= 36; ++ch) {
        map.m_channels.append({ch,
                               QString("Ch %1").arg(ch),
                               startMHz * MHz,
                               (startMHz + 6.0) * MHz,
                               "UHF"});
        startMHz += 6.0;
    }

    return map;
}

TvChannelMap TvChannelMap::euDvbt()
{
    TvChannelMap map;
    constexpr double MHz = 1'000'000.0;

    // DVB-T UHF: Channels 21–48 (470–690 MHz, 8 MHz each)
    double startMHz = 470.0;
    for (int ch = 21; ch <= 48; ++ch) {
        map.m_channels.append({ch,
                               QString("Ch %1").arg(ch),
                               startMHz * MHz,
                               (startMHz + 8.0) * MHz,
                               "UHF"});
        startMHz += 8.0;
    }

    return map;
}

TvChannelMap TvChannelMap::jpIsdb()
{
    TvChannelMap map;
    constexpr double MHz = 1'000'000.0;

    // ISDB-T UHF: Channels 13–62 (470–770 MHz, 6 MHz each)
    double startMHz = 470.0;
    for (int ch = 13; ch <= 62; ++ch) {
        map.m_channels.append({ch,
                               QString("Ch %1").arg(ch),
                               startMHz * MHz,
                               (startMHz + 6.0) * MHz,
                               "UHF"});
        startMHz += 6.0;
    }

    return map;
}

QColor TvChannelMap::bandGroupColor(const QString& bandGroup)
{
    if (bandGroup == "VHF-Lo")
        return QColor(200, 160, 60);    // amber
    if (bandGroup == "VHF-Hi")
        return QColor(60, 190, 190);    // teal
    // UHF (default)
    return QColor(180, 120, 200);       // purple
}
