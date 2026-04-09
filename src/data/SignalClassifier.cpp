#include "SignalClassifier.h"

#include <QMap>
#include <QSet>
#include <algorithm>
#include <cmath>

SignalClassifier::Classification SignalClassifier::classify(const DetectedSignal& signal) const
{
    const double freqHz = signal.centerFreqHz;
    const double bwHz = signal.bandwidthHz;
    const double bwKHz = bwHz / 1000.0;

    // Check if the signal falls inside a TV channel
    const TvChannel* tv = m_tvMap.channelAtFrequency(freqHz);

    // Check within known RF bands
    QString bandCategory;
    for (const auto& band : m_bandPlan.bands()) {
        if (freqHz >= band.startFreqHz && freqHz < band.stopFreqHz) {
            bandCategory = band.category;
            break; // Use the first (most specific) match
        }
    }

    // ── Classification Rules ─────────────────────────────────────────────
    // Priority order: most specific (bandwidth-based within known bands) first

    // DTV / Digital Television — wide bandwidth in TV channel
    // DTV signals are typically 6 MHz (ATSC) or 8 MHz (DVB-T) wide
    if (tv && bwKHz > 200.0) {
        return {QString("DTV Ch %1").arg(tv->number),
                QString("Digital TV (Ch %1, %2 kHz BW)")
                    .arg(tv->number).arg(bwKHz, 0, 'f', 0)};
    }

    // Wireless microphone — narrow bandwidth in Part 74 / PMSE band
    // Typical wireless mic bandwidth: 25–200 kHz
    if ((bandCategory == "Part 74" || bandCategory == "PMSE") && bwKHz < 250.0) {
        if (bwKHz < 50.0)
            return {"Wireless Mic",
                    QString("Likely wireless mic (%1 kHz, narrowband)")
                        .arg(bwKHz, 0, 'f', 1)};
        return {"Wireless Mic",
                QString("Possible wireless mic or IEM (%1 kHz BW)")
                    .arg(bwKHz, 0, 'f', 1)};
    }

    // Narrow signal in a TV channel — likely wireless mic operating in white space
    if (tv && bwKHz < 250.0) {
        return {"Wireless Mic",
                QString("Likely wireless mic in TV Ch %1 (%2 kHz BW)")
                    .arg(tv->number).arg(bwKHz, 0, 'f', 1)};
    }

    // LTE / Cellular — typically 5–20 MHz bandwidth in known LTE bands
    if (bandCategory == "LTE") {
        if (bwKHz > 1000.0)
            return {"LTE/Cellular",
                    QString("LTE carrier (%1 MHz BW)")
                        .arg(bwKHz / 1000.0, 0, 'f', 1)};
        return {"LTE/Cellular",
                QString("Possible LTE signal (%1 kHz BW)")
                    .arg(bwKHz, 0, 'f', 0)};
    }

    // ISM band signals — Wi-Fi, Bluetooth, etc.
    if (bandCategory == "ISM") {
        if (freqHz >= 2400e6 && freqHz <= 2500e6) {
            if (bwKHz > 10000.0)
                return {"Wi-Fi", QString("Wi-Fi channel (%1 MHz BW)")
                                     .arg(bwKHz / 1000.0, 0, 'f', 0)};
            return {"ISM 2.4G", QString("2.4 GHz ISM (%1 kHz BW)")
                                    .arg(bwKHz, 0, 'f', 0)};
        }
        if (freqHz >= 902e6 && freqHz <= 928e6)
            return {"ISM 900", QString("900 MHz ISM (%1 kHz BW)")
                                   .arg(bwKHz, 0, 'f', 0)};
        return {"ISM", QString("ISM band signal (%1 kHz BW)")
                           .arg(bwKHz, 0, 'f', 0)};
    }

    // Public Safety
    if (bandCategory == "Public Safety") {
        return {"Public Safety",
                QString("Public safety radio (%1 kHz BW)")
                    .arg(bwKHz, 0, 'f', 0)};
    }

    // Aeronautical
    if (bandCategory == "Aero") {
        return {"Aero", QString("Aeronautical signal (%1 kHz BW)")
                            .arg(bwKHz, 0, 'f', 0)};
    }

    // Generic narrowband — possibly walkie-talkie, pager, etc.
    if (bwKHz < 25.0)
        return {"Narrowband",
                QString("Narrowband signal (%1 kHz BW)")
                    .arg(bwKHz, 0, 'f', 1)};

    // Unknown
    return {"Unknown",
            QString("Unclassified signal (%1 kHz BW)")
                .arg(bwKHz, 0, 'f', 1)};
}

QVector<DetectedSignal> SignalClassifier::groupByChannel(
    const QVector<DetectedSignal>& rawSignals) const
{
    if (rawSignals.isEmpty())
        return rawSignals;

    // ── Step 1: Assign each signal to a TV channel or LTE band ───────────

    // TV channel wins over LTE when they overlap (e.g. 600 MHz spectrum)
    QMap<int, QVector<int>> tvGroups;    // TV channel number → signal indices
    QMap<int, QVector<int>> lteGroups;   // band-plan index  → signal indices
    QSet<int> assigned;

    for (int i = 0; i < rawSignals.size(); ++i) {
        const double freq = rawSignals[i].centerFreqHz;

        if (const TvChannel* tv = m_tvMap.channelAtFrequency(freq)) {
            tvGroups[tv->number].append(i);
            assigned.insert(i);
            continue;
        }

        const auto& bands = m_bandPlan.bands();
        for (int b = 0; b < bands.size(); ++b) {
            if (bands[b].category == "LTE" &&
                freq >= bands[b].startFreqHz && freq < bands[b].stopFreqHz) {
                lteGroups[b].append(i);
                assigned.insert(i);
                break;
            }
        }
    }

    // ── Step 2: Decide which groups to merge ─────────────────────────────

    QSet<int> consumed;
    QVector<DetectedSignal> result;

    // TV channels: merge only if DTV activity confirmed (any peak BW > 200 kHz)
    for (auto it = tvGroups.constBegin(); it != tvGroups.constEnd(); ++it) {
        const auto& indices = it.value();
        bool hasDtv = false;
        for (int idx : indices) {
            if (rawSignals[idx].bandwidthHz > 200000.0) {
                hasDtv = true;
                break;
            }
        }
        if (!hasDtv)
            continue;   // all narrow → keep as individual (possible mics)

        // Merge: find strongest peak and channel boundaries
        double maxAmp = -200.0;
        for (int idx : indices) {
            maxAmp = std::max(maxAmp, rawSignals[idx].peakAmplitudeDbm);
            consumed.insert(idx);
        }

        const TvChannel* tv = m_tvMap.channelAtFrequency(
            rawSignals[indices.first()].centerFreqHz);
        if (!tv)
            continue;

        DetectedSignal grouped;
        grouped.centerFreqHz = (tv->startFreqHz + tv->stopFreqHz) / 2.0;
        grouped.peakAmplitudeDbm = maxAmp;
        grouped.bandwidthHz = tv->stopFreqHz - tv->startFreqHz;
        grouped.label = QString("Ch %1 (%2\u2013%3 MHz)")
                            .arg(tv->number)
                            .arg(tv->startFreqHz / 1e6, 0, 'f', 1)
                            .arg(tv->stopFreqHz / 1e6, 0, 'f', 1);
        result.append(grouped);
    }

    // LTE bands: merge if 2+ signals in same allocation
    const auto& bands = m_bandPlan.bands();
    for (auto it = lteGroups.constBegin(); it != lteGroups.constEnd(); ++it) {
        const auto& indices = it.value();
        if (indices.size() < 2)
            continue;   // single LTE peak → keep as-is

        double maxAmp = -200.0;
        for (int idx : indices) {
            maxAmp = std::max(maxAmp, rawSignals[idx].peakAmplitudeDbm);
            consumed.insert(idx);
        }

        const auto& band = bands[it.key()];
        DetectedSignal grouped;
        grouped.centerFreqHz = (band.startFreqHz + band.stopFreqHz) / 2.0;
        grouped.peakAmplitudeDbm = maxAmp;
        grouped.bandwidthHz = band.stopFreqHz - band.startFreqHz;
        // Clean the band name (may contain \n for overlay display)
        grouped.label = QString(band.name).replace('\n', ' ');
        result.append(grouped);
    }

    // Keep all ungrouped signals unchanged
    for (int i = 0; i < rawSignals.size(); ++i) {
        if (!consumed.contains(i))
            result.append(rawSignals[i]);
    }

    // Sort by frequency
    std::sort(result.begin(), result.end(),
              [](const DetectedSignal& a, const DetectedSignal& b) {
                  return a.centerFreqHz < b.centerFreqHz;
              });

    return result;
}
