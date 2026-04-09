#pragma once

#include <QColor>
#include <QString>
#include <QStringList>
#include <QVector>

struct RfBand
{
    QString name;
    double startFreqHz = 0.0;
    double stopFreqHz = 0.0;
    QString category;   // e.g. "LTE", "Public Safety", "Part 74", "ISM"
    QColor color;
};

class RfBandPlan
{
public:
    RfBandPlan() = default;

    const QVector<RfBand>& bands() const { return m_bands; }
    QVector<RfBand> bandsInRange(double startHz, double stopHz) const;

    static QStringList availableRegions();
    static RfBandPlan forRegion(const QString& region);

    static RfBandPlan us();
    static RfBandPlan eu();

private:
    QVector<RfBand> m_bands;
};
