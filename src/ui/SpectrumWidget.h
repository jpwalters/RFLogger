#pragma once

#include "data/SweepData.h"
#include "data/FrequencyMarker.h"

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

    void setMarkers(const QVector<FrequencyMarker>& markers);
    void setHighlightFrequency(double freqHz);
    void clearHighlight();
    void clearAll();

signals:
    void frequencyClicked(double freqHz);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    void setupPlot();
    void updateMarkerLines();

    QCPItemText* m_coordLabel = nullptr;
    QCPItemLine* m_vCrosshair = nullptr;
    QCPItemLine* m_hCrosshair = nullptr;
    QCPGraph* m_liveGraph = nullptr;
    QCPGraph* m_maxHoldGraph = nullptr;
    QCPGraph* m_averageGraph = nullptr;

    QVector<QCPItemStraightLine*> m_markerLines;
    QVector<QCPItemText*> m_markerLabels;
    QVector<FrequencyMarker> m_markers;

    QCPItemStraightLine* m_highlightLine = nullptr;
    QCPItemText* m_highlightLabel = nullptr;
};
