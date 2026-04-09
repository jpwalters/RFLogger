#pragma once

#include <QColor>
#include <QString>
#include <QStringList>
#include <QVector>

struct TvChannel
{
    int number = 0;
    QString name;
    double startFreqHz = 0.0;
    double stopFreqHz = 0.0;
    QString bandGroup;   // e.g. "VHF-Lo", "VHF-Hi", "UHF"
};

class TvChannelMap
{
public:
    TvChannelMap() = default;

    const QVector<TvChannel>& channels() const { return m_channels; }
    QVector<TvChannel> channelsInRange(double startHz, double stopHz) const;
    const TvChannel* channelAtFrequency(double freqHz) const;

    static QStringList availableRegions();
    static TvChannelMap forRegion(const QString& region);

    static TvChannelMap usAtsc();
    static TvChannelMap euDvbt();
    static TvChannelMap jpIsdb();

    static QColor bandGroupColor(const QString& bandGroup);

private:
    QVector<TvChannel> m_channels;
};
