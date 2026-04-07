#include "WaterfallWidget.h"

#include <QPainter>
#include <QPaintEvent>
#include <algorithm>
#include <cmath>

WaterfallWidget::WaterfallWidget(QWidget* parent)
    : QWidget(parent)
{
    setMinimumHeight(100);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setAutoFillBackground(true);

    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor(10, 10, 10));
    setPalette(pal);
}

void WaterfallWidget::addSweep(const SweepData& sweep)
{
    if (sweep.isEmpty())
        return;

    SweepRow row;
    row.startFreqHz = sweep.startFreqHz();
    row.stepSizeHz = sweep.stepSizeHz();
    row.amplitudes = sweep.amplitudes();

    // Insert at front (newest at top)
    m_rows.prepend(row);

    // Trim to time depth
    while (m_rows.size() > m_timeDepth)
        m_rows.removeLast();

    rebuildImage();
    update();
}

void WaterfallWidget::clear()
{
    m_rows.clear();
    m_image = QImage();
    update();
}

void WaterfallWidget::setAmplitudeRange(double minDbm, double maxDbm)
{
    m_minDbm = minDbm;
    m_maxDbm = maxDbm;
    rebuildImage();
    update();
}

void WaterfallWidget::setTimeDepth(int rows)
{
    m_timeDepth = std::max(10, rows);
    while (m_rows.size() > m_timeDepth)
        m_rows.removeLast();
    rebuildImage();
    update();
}

QColor WaterfallWidget::amplitudeToColor(double dbm) const
{
    // Normalize to 0..1
    double t = (dbm - m_minDbm) / (m_maxDbm - m_minDbm);
    t = std::clamp(t, 0.0, 1.0);

    // Color gradient: dark blue → cyan → green → yellow → red
    int r, g, b;
    if (t < 0.25) {
        double s = t / 0.25;
        r = 0;
        g = static_cast<int>(s * 255);
        b = static_cast<int>(128 + s * 127);
    } else if (t < 0.5) {
        double s = (t - 0.25) / 0.25;
        r = 0;
        g = 255;
        b = static_cast<int>(255 * (1.0 - s));
    } else if (t < 0.75) {
        double s = (t - 0.5) / 0.25;
        r = static_cast<int>(s * 255);
        g = 255;
        b = 0;
    } else {
        double s = (t - 0.75) / 0.25;
        r = 255;
        g = static_cast<int>(255 * (1.0 - s));
        b = 0;
    }

    return QColor(r, g, b);
}

void WaterfallWidget::rebuildImage()
{
    if (m_rows.isEmpty())
        return;

    int cols = static_cast<int>(m_rows.first().amplitudes.size());
    int rows = static_cast<int>(m_rows.size());

    if (cols <= 0 || rows <= 0)
        return;

    m_image = QImage(cols, rows, QImage::Format_RGB32);

    for (int y = 0; y < rows; ++y) {
        const auto& row = m_rows[y];
        auto* scanLine = reinterpret_cast<QRgb*>(m_image.scanLine(y));

        for (int x = 0; x < cols && x < static_cast<int>(row.amplitudes.size()); ++x) {
            QColor c = amplitudeToColor(row.amplitudes[x]);
            scanLine[x] = c.rgb();
        }
        // Fill remaining pixels if row is shorter
        for (int x = static_cast<int>(row.amplitudes.size()); x < cols; ++x) {
            scanLine[x] = qRgb(10, 10, 10);
        }
    }
}

void WaterfallWidget::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    painter.fillRect(rect(), QColor(10, 10, 10));

    if (m_image.isNull())
        return;

    // Scale the waterfall image to fill the widget
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    painter.drawImage(rect(), m_image);
}

void WaterfallWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
}
