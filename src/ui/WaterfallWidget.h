#pragma once

#include "data/SweepData.h"

#include <QWidget>
#include <QImage>
#include <QVector>

class WaterfallWidget : public QWidget
{
    Q_OBJECT

public:
    explicit WaterfallWidget(QWidget* parent = nullptr);

    void addSweep(const SweepData& sweep);
    void clear();

    void setAmplitudeRange(double minDbm, double maxDbm);
    void setTimeDepth(int rows);

    int timeDepth() const { return m_timeDepth; }

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    QColor amplitudeToColor(double dbm) const;
    void rebuildImage();

    struct SweepRow {
        double startFreqHz;
        double stepSizeHz;
        QVector<double> amplitudes;
    };

    QVector<SweepRow> m_rows;
    QImage m_image;
    int m_timeDepth = 200;
    double m_minDbm = -120.0;
    double m_maxDbm = -20.0;
};
