#include "SpectrumWidget.h"

#include <QMouseEvent>

SpectrumWidget::SpectrumWidget(QWidget* parent)
    : QCustomPlot(parent)
{
    setupPlot();
}

void SpectrumWidget::setupPlot()
{
    // Dark theme colors
    setBackground(QBrush(QColor(22, 22, 22)));

    xAxis->setBasePen(QPen(QColor(100, 100, 100)));
    xAxis->setTickPen(QPen(QColor(100, 100, 100)));
    xAxis->setSubTickPen(QPen(QColor(60, 60, 60)));
    xAxis->setTickLabelColor(QColor(180, 180, 180));
    xAxis->setLabelColor(QColor(180, 180, 180));
    xAxis->setLabel("Frequency (MHz)");
    xAxis->grid()->setPen(QPen(QColor(50, 50, 50)));
    xAxis->grid()->setSubGridPen(QPen(QColor(35, 35, 35)));
    xAxis->grid()->setSubGridVisible(true);

    yAxis->setBasePen(QPen(QColor(100, 100, 100)));
    yAxis->setTickPen(QPen(QColor(100, 100, 100)));
    yAxis->setSubTickPen(QPen(QColor(60, 60, 60)));
    yAxis->setTickLabelColor(QColor(180, 180, 180));
    yAxis->setLabelColor(QColor(180, 180, 180));
    yAxis->setLabel("Amplitude (dBm)");
    yAxis->grid()->setPen(QPen(QColor(50, 50, 50)));
    yAxis->grid()->setSubGridPen(QPen(QColor(35, 35, 35)));
    yAxis->grid()->setSubGridVisible(true);

    // Default ranges
    xAxis->setRange(470, 700);
    yAxis->setRange(-120, 5);

    // Show ticks every 5 dBm for easier readability
    QSharedPointer<QCPAxisTickerFixed> yTicker(new QCPAxisTickerFixed);
    yTicker->setTickStep(5.0);
    yAxis->setTicker(yTicker);

    // Show ticks every 10 MHz on the X axis
    QSharedPointer<QCPAxisTickerFixed> xTicker(new QCPAxisTickerFixed);
    xTicker->setTickStep(10.0);
    xAxis->setTicker(xTicker);

    // Coordinate readout label (follows mouse)
    m_coordLabel = new QCPItemText(this);
    m_coordLabel->setPositionAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_coordLabel->position->setType(QCPItemPosition::ptAbsolute);
    m_coordLabel->setFont(QFont(font().family(), 9));
    m_coordLabel->setColor(QColor(220, 220, 220));
    m_coordLabel->setPadding(QMargins(4, 2, 4, 2));
    m_coordLabel->setBrush(QBrush(QColor(30, 30, 30, 180)));
    m_coordLabel->setVisible(false);

    // Crosshair lines (follow mouse across full plot area)
    QPen crosshairPen(QColor(255, 255, 255, 120), 1, Qt::DashLine);

    m_vCrosshair = new QCPItemLine(this);
    m_vCrosshair->setPen(crosshairPen);
    m_vCrosshair->setVisible(false);

    m_hCrosshair = new QCPItemLine(this);
    m_hCrosshair->setPen(crosshairPen);
    m_hCrosshair->setVisible(false);

    setMouseTracking(true);

    // Enable zoom and pan
    setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    axisRect()->setRangeDrag(Qt::Horizontal | Qt::Vertical);
    axisRect()->setRangeZoom(Qt::Horizontal | Qt::Vertical);

    // Create graph layers
    m_liveGraph = addGraph();
    m_liveGraph->setPen(QPen(QColor(0, 255, 128), 1.5));
    m_liveGraph->setName("Live");

    m_maxHoldGraph = addGraph();
    m_maxHoldGraph->setPen(QPen(QColor(255, 60, 60), 1.2));
    m_maxHoldGraph->setName("Max Hold");

    m_averageGraph = addGraph();
    m_averageGraph->setPen(QPen(QColor(60, 140, 255), 1.2));
    m_averageGraph->setName("Average");

    // Legend
    legend->setVisible(true);
    legend->setBrush(QBrush(QColor(30, 30, 30, 200)));
    legend->setTextColor(QColor(180, 180, 180));
    legend->setBorderPen(QPen(QColor(80, 80, 80)));
    QFont legendFont = font();
    legendFont.setPointSize(9);
    legend->setFont(legendFont);
}

void SpectrumWidget::updateLive(const SweepData& sweep)
{
    if (sweep.isEmpty())
        return;

    QVector<double> freqs(sweep.count());
    for (int i = 0; i < sweep.count(); ++i)
        freqs[i] = sweep.frequencyAtIndex(i) / 1'000'000.0; // Convert to MHz

    m_liveGraph->setData(freqs, sweep.amplitudes());

    // Auto-fit X range on first data
    if (xAxis->range().size() < 0.001) {
        xAxis->setRange(freqs.first(), freqs.last());
    }

    replot(QCustomPlot::rpQueuedReplot);
}

void SpectrumWidget::updateMaxHold(const SweepData& sweep)
{
    if (sweep.isEmpty())
        return;

    QVector<double> freqs(sweep.count());
    for (int i = 0; i < sweep.count(); ++i)
        freqs[i] = sweep.frequencyAtIndex(i) / 1'000'000.0;

    m_maxHoldGraph->setData(freqs, sweep.amplitudes());
    replot(QCustomPlot::rpQueuedReplot);
}

void SpectrumWidget::updateAverage(const SweepData& sweep)
{
    if (sweep.isEmpty())
        return;

    QVector<double> freqs(sweep.count());
    for (int i = 0; i < sweep.count(); ++i)
        freqs[i] = sweep.frequencyAtIndex(i) / 1'000'000.0;

    m_averageGraph->setData(freqs, sweep.amplitudes());
    replot(QCustomPlot::rpQueuedReplot);
}

void SpectrumWidget::setShowLive(bool show)
{
    m_liveGraph->setVisible(show);
    replot();
}

void SpectrumWidget::setShowMaxHold(bool show)
{
    m_maxHoldGraph->setVisible(show);
    replot();
}

void SpectrumWidget::setShowAverage(bool show)
{
    m_averageGraph->setVisible(show);
    replot();
}

void SpectrumWidget::setFrequencyRange(double startMHz, double stopMHz)
{
    xAxis->setRange(startMHz, stopMHz);
    replot();
}

void SpectrumWidget::setAmplitudeRange(double minDbm, double maxDbm)
{
    yAxis->setRange(minDbm, maxDbm);
    replot();
}

void SpectrumWidget::setMarkers(const QVector<FrequencyMarker>& markers)
{
    m_markers = markers;
    updateMarkerLines();
}

void SpectrumWidget::clearAll()
{
    m_liveGraph->data()->clear();
    m_maxHoldGraph->data()->clear();
    m_averageGraph->data()->clear();
    clearHighlight();
    replot();
}

void SpectrumWidget::setHighlightFrequency(double freqHz)
{
    clearHighlight();

    double freqMHz = freqHz / 1'000'000.0;

    m_highlightLine = new QCPItemStraightLine(this);
    m_highlightLine->point1->setCoords(freqMHz, -200);
    m_highlightLine->point2->setCoords(freqMHz, 100);
    m_highlightLine->setPen(QPen(QColor(0, 255, 128), 2, Qt::SolidLine));

    m_highlightLabel = new QCPItemText(this);
    m_highlightLabel->setPositionAlignment(Qt::AlignTop | Qt::AlignHCenter);
    m_highlightLabel->position->setCoords(freqMHz, yAxis->range().upper - 2);
    m_highlightLabel->setText(QString("%1 MHz").arg(freqMHz, 0, 'f', 3));
    m_highlightLabel->setColor(QColor(0, 255, 128));
    m_highlightLabel->setFont(QFont(font().family(), 9, QFont::Bold));
    m_highlightLabel->setPadding(QMargins(4, 2, 4, 2));
    m_highlightLabel->setBrush(QBrush(QColor(0, 40, 0, 180)));

    replot();
}

void SpectrumWidget::clearHighlight()
{
    if (m_highlightLine) {
        removeItem(m_highlightLine);
        m_highlightLine = nullptr;
    }
    if (m_highlightLabel) {
        removeItem(m_highlightLabel);
        m_highlightLabel = nullptr;
    }
    replot();
}

void SpectrumWidget::updateMarkerLines()
{
    // Remove existing marker items
    for (auto* line : m_markerLines)
        removeItem(line);
    for (auto* label : m_markerLabels)
        removeItem(label);
    m_markerLines.clear();
    m_markerLabels.clear();

    for (const auto& marker : m_markers) {
        if (!marker.visible)
            continue;

        double freqMHz = marker.frequencyHz / 1'000'000.0;

        auto* line = new QCPItemStraightLine(this);
        line->point1->setCoords(freqMHz, -200);
        line->point2->setCoords(freqMHz, 100);
        line->setPen(QPen(marker.color, 1, Qt::DashLine));
        m_markerLines.append(line);

        if (!marker.label.isEmpty()) {
            auto* label = new QCPItemText(this);
            label->setPositionAlignment(Qt::AlignBottom | Qt::AlignHCenter);
            label->position->setCoords(freqMHz, yAxis->range().upper - 2);
            label->setText(marker.label);
            label->setColor(marker.color);
            label->setFont(QFont(font().family(), 8));
            label->setPadding(QMargins(2, 2, 2, 2));
            m_markerLabels.append(label);
        }
    }

    replot();
}

void SpectrumWidget::mouseMoveEvent(QMouseEvent* event)
{
    double freqMHz = xAxis->pixelToCoord(event->pos().x());
    double ampDbm = yAxis->pixelToCoord(event->pos().y());

    if (axisRect()->rect().contains(event->pos())) {
        QString text = QString("%1 MHz, %2 dBm")
                           .arg(freqMHz, 0, 'f', 3)
                           .arg(ampDbm, 0, 'f', 1);
        m_coordLabel->setText(text);

        // Estimate label size and keep it inside the plot area
        QFontMetrics fm(m_coordLabel->font());
        int labelW = fm.horizontalAdvance(text) + 12; // padding
        int labelH = fm.height() + 8;
        QRect plotRect = axisRect()->rect();

        double lx = event->pos().x() + 12;
        double ly = event->pos().y() - 12;

        // Flip to left of cursor if it would overflow the right edge
        if (lx + labelW > plotRect.right())
            lx = event->pos().x() - 12 - labelW;

        // Flip below cursor if it would overflow the top edge
        if (ly - labelH < plotRect.top())
            ly = event->pos().y() + 12 + labelH;

        m_coordLabel->position->setCoords(lx, ly);
        m_coordLabel->setPositionAlignment(Qt::AlignLeft | Qt::AlignBottom);
        m_coordLabel->setVisible(true);

        // Vertical crosshair (cursor down to bottom axis)
        m_vCrosshair->start->setCoords(freqMHz, ampDbm);
        m_vCrosshair->end->setCoords(freqMHz, yAxis->range().lower);
        m_vCrosshair->setVisible(true);

        // Horizontal crosshair (cursor left to Y axis)
        m_hCrosshair->start->setCoords(freqMHz, ampDbm);
        m_hCrosshair->end->setCoords(xAxis->range().lower, ampDbm);
        m_hCrosshair->setVisible(true);
    } else {
        m_coordLabel->setVisible(false);
        m_vCrosshair->setVisible(false);
        m_hCrosshair->setVisible(false);
    }
    replot(QCustomPlot::rpQueuedReplot);

    QCustomPlot::mouseMoveEvent(event);
}

void SpectrumWidget::leaveEvent(QEvent* event)
{
    m_coordLabel->setVisible(false);
    m_vCrosshair->setVisible(false);
    m_hCrosshair->setVisible(false);
    replot(QCustomPlot::rpQueuedReplot);
    QCustomPlot::leaveEvent(event);
}

void SpectrumWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::MiddleButton) {
        double freqMHz = xAxis->pixelToCoord(event->pos().x());
        emit frequencyClicked(freqMHz * 1'000'000.0);
    }

    QCustomPlot::mousePressEvent(event);
}
