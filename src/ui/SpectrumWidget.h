#pragma once

#include "data/SweepData.h"
#include "data/FrequencyMarker.h"
#include "data/TvChannelMap.h"
#include "data/RfBandPlan.h"

#include <qcustomplot.h>
#include <QVector>

class SpectrumWidget : public QCustomPlot
{
    Q_OBJECT

public:
    explicit SpectrumWidget(QWidget* parent = nullptr);

    void updateLive(const SweepData& sweep);
    void updateMaxHold(const SweepData& sweep);
    void updateAverage(const SweepData& sweep);

    void setShowLive(bool show);
    void setShowMaxHold(bool show);
    void setShowAverage(bool show);

    void setFrequencyRange(double startMHz, double stopMHz);
    void setAmplitudeRange(double minDbm, double maxDbm);
    void autoFitAmplitude(const SweepData& sweep);

    void setMarkers(const QVector<FrequencyMarker>& markers);
    void setHighlightFrequency(double freqHz);
    void clearHighlight();
    void clearAll();
    void setCrosshairVisible(bool visible);

    void setTvChannelMap(const TvChannelMap& map);
    void setShowTvBands(bool show);
    void highlightTvChannel(int channelNumber);
    void clearTvHighlight();

    // Reference trace overlay
    void setReferenceTrace(const SweepData& sweep, const QString& label);
    void clearReferenceTrace();
    void setShowReference(bool show);
    bool hasReferenceTrace() const { return !m_referenceData.isEmpty(); }

    // Band plan overlay
    void setShowBandPlan(bool show);
    void setBandPlanRegion(const QString& region);

    // Imported CSV trace overlays
    struct ImportedTrace {
        int id = 0;
        QString name;
        SweepData sweep;
        QCPGraph* graph = nullptr;
        bool visible = true;
    };

    int addImportedTrace(const SweepData& sweep, const QString& name);
    void removeImportedTrace(int id);
    void removeAllImportedTraces();
    void setImportedTraceVisible(int id, bool visible);
    const QVector<ImportedTrace>& importedTraces() const { return m_importedTraces; }

signals:
    void frequencyClicked(double freqHz);
    void tvChannelClicked(int channelNumber);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    void setupPlot();
    void updateMarkerLines();
    void updateTvBandOverlay();
    void clearTvBandItems();
    void updateBandPlanOverlay();
    void clearBandPlanItems();

    QCPItemText* m_coordLabel = nullptr;
    QCPItemLine* m_vCrosshair = nullptr;
    QCPItemLine* m_hCrosshair = nullptr;
    QCPGraph* m_liveGraph = nullptr;
    QCPGraph* m_maxHoldGraph = nullptr;
    QCPGraph* m_averageGraph = nullptr;

    // Reference trace overlay
    QCPGraph* m_referenceGraph = nullptr;
    SweepData m_referenceData;

    QVector<QCPItemStraightLine*> m_markerLines;
    QVector<QCPItemText*> m_markerLabels;
    QVector<FrequencyMarker> m_markers;

    QCPItemStraightLine* m_highlightLine = nullptr;
    QCPItemText* m_highlightLabel = nullptr;

    // TV channel band overlay
    QVector<QCPItemRect*> m_tvBandRects;
    QVector<QCPItemText*> m_tvBandLabels;
    QCPItemRect* m_tvHighlightRect = nullptr;
    QCPItemText* m_tvHighlightLabel = nullptr;
    TvChannelMap m_tvChannelMap;
    bool m_showTvBands = false;
    int m_highlightedTvChannel = -1;

    // Band plan overlay
    QVector<QCPItemRect*> m_bandPlanRects;
    QVector<QCPItemText*> m_bandPlanLabels;
    RfBandPlan m_bandPlan;
    bool m_showBandPlan = false;

    // Imported CSV trace overlays
    QVector<ImportedTrace> m_importedTraces;
    int m_nextTraceId = 1;

    // Distinct colors for imported traces (cycled)
    static QColor traceColor(int index);

    bool m_crosshairEnabled = true;
};
