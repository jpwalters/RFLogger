#include "SpectrumWidget.h"

#include <QMouseEvent>
#include <cmath>

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

    // Create a layer for TV band rectangles behind the graph data
    addLayer("tvbands", layer("grid"), QCustomPlot::limAbove);

    // Enable zoom and pan
    setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    axisRect()->setRangeDrag(Qt::Horizontal | Qt::Vertical);
    axisRect()->setRangeZoom(Qt::Horizontal | Qt::Vertical);

    // Refresh TV band overlay when the user pans or zooms
    connect(xAxis, QOverload<const QCPRange&>::of(&QCPAxis::rangeChanged),
            this, [this]() {
                updateTvBandOverlay();
                updateBandPlanOverlay();
            });

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

    m_referenceGraph = addGraph();
    m_referenceGraph->setPen(QPen(QColor(255, 180, 0, 160), 1.2, Qt::DashLine));
    m_referenceGraph->setName("Reference");
    m_referenceGraph->setVisible(false);

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

void SpectrumWidget::autoFitAmplitude(const SweepData& sweep)
{
    if (sweep.isEmpty())
        return;

    double minDb = sweep.minAmplitude();
    double maxDb = sweep.maxAmplitude();

    // Add padding: 5 dBm above, 10 dBm below for visual breathing room
    maxDb += 5.0;
    minDb -= 10.0;

    // Round to nearest 5 dBm for clean tick marks
    minDb = std::floor(minDb / 5.0) * 5.0;
    maxDb = std::ceil(maxDb / 5.0) * 5.0;

    // Enforce a minimum span of 20 dBm so the graph isn't too cramped
    if (maxDb - minDb < 20.0) {
        double mid = (maxDb + minDb) / 2.0;
        minDb = mid - 10.0;
        maxDb = mid + 10.0;
    }

    yAxis->setRange(minDb, maxDb);
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
    // Note: reference trace is intentionally preserved across clears
    clearHighlight();
    clearTvHighlight();
    replot();
}

void SpectrumWidget::setCrosshairVisible(bool visible)
{
    m_crosshairEnabled = visible;
    if (!visible) {
        m_coordLabel->setVisible(false);
        m_vCrosshair->setVisible(false);
        m_hCrosshair->setVisible(false);
        replot(QCustomPlot::rpQueuedReplot);
    }
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
    if (!m_crosshairEnabled) {
        QCustomPlot::mouseMoveEvent(event);
        return;
    }

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

    // Left-click: select a TV channel band (when overlay is visible)
    if (event->button() == Qt::LeftButton && m_showTvBands) {
        double freqMHz = xAxis->pixelToCoord(event->pos().x());
        double freqHz = freqMHz * 1'000'000.0;
        const TvChannel* ch = m_tvChannelMap.channelAtFrequency(freqHz);
        if (ch)
            emit tvChannelClicked(ch->number);
        else
            clearTvHighlight();
    }

    QCustomPlot::mousePressEvent(event);
}

// ── TV Channel Band Overlay ──────────────────────────────────────────────────

void SpectrumWidget::setTvChannelMap(const TvChannelMap& map)
{
    m_tvChannelMap = map;
    m_highlightedTvChannel = -1;
    updateTvBandOverlay();
}

void SpectrumWidget::setShowTvBands(bool show)
{
    m_showTvBands = show;
    if (!show) {
        clearTvBandItems();
        clearTvHighlight();
        replot();
    } else {
        updateTvBandOverlay();
    }
}

void SpectrumWidget::updateTvBandOverlay()
{
    clearTvBandItems();

    if (!m_showTvBands) {
        replot(QCustomPlot::rpQueuedReplot);
        return;
    }

    double startHz = xAxis->range().lower * 1'000'000.0;
    double stopHz  = xAxis->range().upper * 1'000'000.0;
    auto visible = m_tvChannelMap.channelsInRange(startHz, stopHz);

    QCPLayer* bandLayer = layer("tvbands");

    for (const auto& ch : visible) {
        double startMHz = ch.startFreqHz / 1'000'000.0;
        double stopMHz  = ch.stopFreqHz  / 1'000'000.0;

        QColor baseColor = TvChannelMap::bandGroupColor(ch.bandGroup);

        // Alternate even/odd channels for visual distinction
        int alpha = (ch.number % 2 == 0) ? 25 : 35;
        QColor fillColor(baseColor.red(), baseColor.green(), baseColor.blue(), alpha);
        QColor borderColor(baseColor.red(), baseColor.green(), baseColor.blue(), 60);

        auto* rect = new QCPItemRect(this);
        rect->setLayer(bandLayer);
        rect->topLeft->setCoords(startMHz, 200);
        rect->bottomRight->setCoords(stopMHz, -200);
        rect->setPen(QPen(borderColor, 1));
        rect->setBrush(QBrush(fillColor));
        m_tvBandRects.append(rect);

        // Channel number label at top of the band
        // Only show label if the channel is wide enough on screen
        double pixelWidth = xAxis->coordToPixel(stopMHz) - xAxis->coordToPixel(startMHz);
        if (pixelWidth >= 20) {
            auto* label = new QCPItemText(this);
            label->setLayer(bandLayer);
            label->setPositionAlignment(Qt::AlignTop | Qt::AlignHCenter);
            label->position->setCoords((startMHz + stopMHz) / 2.0, yAxis->range().upper - 1);
            label->setText(QString::number(ch.number));
            label->setColor(QColor(baseColor.red(), baseColor.green(), baseColor.blue(), 140));
            label->setFont(QFont(font().family(), 7));
            label->setPadding(QMargins(1, 0, 1, 0));
            m_tvBandLabels.append(label);
        }
    }

    // Re-create highlight if one was active
    if (m_highlightedTvChannel >= 0)
        highlightTvChannel(m_highlightedTvChannel);

    replot(QCustomPlot::rpQueuedReplot);
}

void SpectrumWidget::clearTvBandItems()
{
    for (auto* rect : m_tvBandRects)
        removeItem(rect);
    for (auto* label : m_tvBandLabels)
        removeItem(label);
    m_tvBandRects.clear();
    m_tvBandLabels.clear();
}

void SpectrumWidget::highlightTvChannel(int channelNumber)
{
    // Remove previous highlight items (but keep the tracked channel number)
    if (m_tvHighlightRect) {
        removeItem(m_tvHighlightRect);
        m_tvHighlightRect = nullptr;
    }
    if (m_tvHighlightLabel) {
        removeItem(m_tvHighlightLabel);
        m_tvHighlightLabel = nullptr;
    }

    // Find the channel
    const TvChannel* ch = nullptr;
    for (const auto& c : m_tvChannelMap.channels()) {
        if (c.number == channelNumber) {
            ch = &c;
            break;
        }
    }
    if (!ch) {
        m_highlightedTvChannel = -1;
        replot();
        return;
    }

    m_highlightedTvChannel = channelNumber;

    double startMHz = ch->startFreqHz / 1'000'000.0;
    double stopMHz  = ch->stopFreqHz  / 1'000'000.0;
    QColor baseColor = TvChannelMap::bandGroupColor(ch->bandGroup);

    m_tvHighlightRect = new QCPItemRect(this);
    m_tvHighlightRect->topLeft->setCoords(startMHz, 200);
    m_tvHighlightRect->bottomRight->setCoords(stopMHz, -200);
    m_tvHighlightRect->setPen(QPen(QColor(baseColor.red(), baseColor.green(), baseColor.blue(), 180), 2));
    m_tvHighlightRect->setBrush(QBrush(QColor(baseColor.red(), baseColor.green(), baseColor.blue(), 70)));

    m_tvHighlightLabel = new QCPItemText(this);
    m_tvHighlightLabel->setPositionAlignment(Qt::AlignTop | Qt::AlignHCenter);
    m_tvHighlightLabel->position->setCoords((startMHz + stopMHz) / 2.0, yAxis->range().upper - 4);
    m_tvHighlightLabel->setText(QString("Ch %1\n%2\u2013%3 MHz")
                                    .arg(ch->number)
                                    .arg(startMHz, 0, 'f', 0)
                                    .arg(stopMHz, 0, 'f', 0));
    m_tvHighlightLabel->setColor(QColor(255, 255, 255));
    m_tvHighlightLabel->setFont(QFont(font().family(), 9, QFont::Bold));
    m_tvHighlightLabel->setPadding(QMargins(4, 2, 4, 2));
    m_tvHighlightLabel->setBrush(QBrush(QColor(baseColor.red(), baseColor.green(), baseColor.blue(), 180)));

    replot();
}

void SpectrumWidget::clearTvHighlight()
{
    if (m_tvHighlightRect) {
        removeItem(m_tvHighlightRect);
        m_tvHighlightRect = nullptr;
    }
    if (m_tvHighlightLabel) {
        removeItem(m_tvHighlightLabel);
        m_tvHighlightLabel = nullptr;
    }
    m_highlightedTvChannel = -1;
    replot();
}

// ── Reference Trace Overlay ─────────────────────────────────────────────────

void SpectrumWidget::setReferenceTrace(const SweepData& sweep, const QString& /*label*/)
{
    m_referenceData = sweep;

    if (sweep.isEmpty()) {
        m_referenceGraph->data()->clear();
        m_referenceGraph->setVisible(false);
        replot();
        return;
    }

    QVector<double> freqs(sweep.count());
    for (int i = 0; i < sweep.count(); ++i)
        freqs[i] = sweep.frequencyAtIndex(i) / 1'000'000.0;

    m_referenceGraph->setData(freqs, sweep.amplitudes());
    m_referenceGraph->setVisible(true);
    replot(QCustomPlot::rpQueuedReplot);
}

void SpectrumWidget::clearReferenceTrace()
{
    m_referenceData = SweepData();
    m_referenceGraph->data()->clear();
    m_referenceGraph->setVisible(false);
    replot();
}

void SpectrumWidget::setShowReference(bool show)
{
    m_referenceGraph->setVisible(show && !m_referenceData.isEmpty());
    replot();
}

// ── Band Plan Overlay ────────────────────────────────────────────────────────

void SpectrumWidget::setShowBandPlan(bool show)
{
    m_showBandPlan = show;
    if (!show) {
        clearBandPlanItems();
        replot();
    } else {
        updateBandPlanOverlay();
    }
}

void SpectrumWidget::setBandPlanRegion(const QString& region)
{
    m_bandPlan = RfBandPlan::forRegion(region);
    updateBandPlanOverlay();
}

void SpectrumWidget::updateBandPlanOverlay()
{
    clearBandPlanItems();

    if (!m_showBandPlan) {
        replot(QCustomPlot::rpQueuedReplot);
        return;
    }

    double startHz = xAxis->range().lower * 1'000'000.0;
    double stopHz  = xAxis->range().upper * 1'000'000.0;
    auto visible = m_bandPlan.bandsInRange(startHz, stopHz);

    QCPLayer* bandLayer = layer("tvbands"); // Reuse the same layer

    for (const auto& band : visible) {
        double startMHz = band.startFreqHz / 1'000'000.0;
        double stopMHz  = band.stopFreqHz  / 1'000'000.0;

        QColor fillColor(band.color.red(), band.color.green(), band.color.blue(), 25);
        QColor borderColor(band.color.red(), band.color.green(), band.color.blue(), 50);

        auto* rect = new QCPItemRect(this);
        rect->setLayer(bandLayer);
        rect->topLeft->setCoords(startMHz, 200);
        rect->bottomRight->setCoords(stopMHz, -200);
        rect->setPen(QPen(borderColor, 1));
        rect->setBrush(QBrush(fillColor));
        m_bandPlanRects.append(rect);

        // Show label only if band is wide enough on screen
        double pixelWidth = xAxis->coordToPixel(stopMHz) - xAxis->coordToPixel(startMHz);
        if (pixelWidth >= 40) {
            auto* label = new QCPItemText(this);
            label->setLayer(bandLayer);
            label->setPositionAlignment(Qt::AlignBottom | Qt::AlignHCenter);
            label->position->setCoords((startMHz + stopMHz) / 2.0, yAxis->range().lower + 2);
            label->setText(band.name);
            label->setColor(QColor(band.color.red(), band.color.green(), band.color.blue(), 160));
            label->setFont(QFont(font().family(), 7));
            label->setPadding(QMargins(1, 0, 1, 0));
            m_bandPlanLabels.append(label);
        }
    }

    replot(QCustomPlot::rpQueuedReplot);
}

void SpectrumWidget::clearBandPlanItems()
{
    for (auto* rect : m_bandPlanRects)
        removeItem(rect);
    for (auto* label : m_bandPlanLabels)
        removeItem(label);
    m_bandPlanRects.clear();
    m_bandPlanLabels.clear();
}

// ── Imported CSV Trace Overlays ──────────────────────────────────────────────

QColor SpectrumWidget::traceColor(int index)
{
    static const QColor palette[] = {
        QColor(0, 200, 200),      // cyan
        QColor(200, 100, 255),    // purple
        QColor(255, 160, 50),     // orange
        QColor(100, 255, 100),    // lime
        QColor(255, 100, 180),    // pink
        QColor(180, 180, 50),     // olive
        QColor(100, 180, 255),    // sky
        QColor(255, 255, 100),    // yellow
    };
    constexpr int N = sizeof(palette) / sizeof(palette[0]);
    return palette[index % N];
}

int SpectrumWidget::addImportedTrace(const SweepData& sweep, const QString& name)
{
    if (sweep.isEmpty())
        return -1;

    ImportedTrace trace;
    trace.id = m_nextTraceId++;
    trace.name = name;
    trace.sweep = sweep;

    QColor color = traceColor(m_importedTraces.size());

    trace.graph = addGraph();
    trace.graph->setPen(QPen(color, 1.4, Qt::DashDotLine));
    trace.graph->setName(name);
    trace.graph->setVisible(true);

    QVector<double> freqs(sweep.count());
    for (int i = 0; i < sweep.count(); ++i)
        freqs[i] = sweep.frequencyAtIndex(i) / 1'000'000.0;
    trace.graph->setData(freqs, sweep.amplitudes());

    m_importedTraces.append(trace);
    replot(QCustomPlot::rpQueuedReplot);
    return trace.id;
}

void SpectrumWidget::removeImportedTrace(int id)
{
    for (int i = 0; i < m_importedTraces.size(); ++i) {
        if (m_importedTraces[i].id == id) {
            removeGraph(m_importedTraces[i].graph);
            m_importedTraces.removeAt(i);
            replot(QCustomPlot::rpQueuedReplot);
            return;
        }
    }
}

void SpectrumWidget::removeAllImportedTraces()
{
    for (auto& trace : m_importedTraces)
        removeGraph(trace.graph);
    m_importedTraces.clear();
    replot(QCustomPlot::rpQueuedReplot);
}

void SpectrumWidget::setImportedTraceVisible(int id, bool visible)
{
    for (auto& trace : m_importedTraces) {
        if (trace.id == id) {
            trace.visible = visible;
            trace.graph->setVisible(visible);
            replot(QCustomPlot::rpQueuedReplot);
            return;
        }
    }
}
