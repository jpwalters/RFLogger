#pragma once

#include "DetectedSignal.h"
#include "RfBandPlan.h"
#include "TvChannelMap.h"

#include <QString>
#include <QVector>

class SignalClassifier
{
public:
    SignalClassifier() = default;

    struct Classification {
        QString type;          // e.g. "Wireless Mic", "DTV", "LTE"
        QString description;   // e.g. "Likely wireless microphone (25 kHz BW in Part 74 band)"
    };

    Classification classify(const DetectedSignal& signal) const;

    /// Group raw detected peaks into logical signals.
    /// Merges multiple peaks in the same active DTV channel into one entry,
    /// and multiple peaks in the same LTE band into one entry.
    QVector<DetectedSignal> groupByChannel(const QVector<DetectedSignal>& rawSignals) const;

    void setTvChannelMap(const TvChannelMap& map) { m_tvMap = map; }
    void setBandPlan(const RfBandPlan& plan) { m_bandPlan = plan; }

private:
    TvChannelMap m_tvMap;
    RfBandPlan m_bandPlan;
};
