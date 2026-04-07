#include "FrequencyListPanel.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QHeaderView>

FrequencyListPanel::FrequencyListPanel(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);

    // Threshold control
    auto* threshGroup = new QGroupBox(tr("Peak Detection"));
    auto* threshLayout = new QVBoxLayout(threshGroup);

    auto* sliderRow = new QHBoxLayout;
    sliderRow->addWidget(new QLabel(tr("Threshold:")));
    m_thresholdSlider = new QSlider(Qt::Horizontal);
    m_thresholdSlider->setRange(3, 40);
    m_thresholdSlider->setValue(static_cast<int>(m_detector.thresholdDb()));
    sliderRow->addWidget(m_thresholdSlider);
    m_thresholdLabel = new QLabel(tr("%1 dB").arg(m_detector.thresholdDb(), 0, 'f', 0));
    m_thresholdLabel->setMinimumWidth(45);
    sliderRow->addWidget(m_thresholdLabel);
    threshLayout->addLayout(sliderRow);

    auto* controlRow = new QHBoxLayout;
    m_autoRefreshBox = new QCheckBox(tr("Auto-refresh"));
    m_autoRefreshBox->setChecked(true);
    controlRow->addWidget(m_autoRefreshBox);
    m_detectBtn = new QPushButton(tr("Detect Now"));
    controlRow->addWidget(m_detectBtn);
    threshLayout->addLayout(controlRow);

    layout->addWidget(threshGroup);

    // Frequency table
    m_table = new QTableWidget(0, 4);
    m_table->setHorizontalHeaderLabels({tr("Frequency"), tr("Amplitude"), tr("Bandwidth"), tr("Listen")});
    m_table->horizontalHeader()->setStretchLastSection(false);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->setVisible(false);
    layout->addWidget(m_table);

    // Stop button
    m_stopBtn = new QPushButton(tr("Stop Listening"));
    m_stopBtn->setEnabled(false);
    layout->addWidget(m_stopBtn);

    // Connections
    connect(m_thresholdSlider, &QSlider::valueChanged, this, &FrequencyListPanel::onThresholdChanged);
    connect(m_detectBtn, &QPushButton::clicked, this, &FrequencyListPanel::onDetectClicked);
    connect(m_stopBtn, &QPushButton::clicked, this, [this]() {
        m_listeningRow = -1;
        m_stopBtn->setEnabled(false);
        updateTable();
        emit stopListenRequested();
    });
    connect(m_table, &QTableWidget::currentCellChanged, this, [this](int row, int /*col*/, int /*prevRow*/, int /*prevCol*/) {
        if (row >= 0 && row < m_signals.size())
            emit frequencySelected(m_signals[row].centerFreqHz);
        else
            emit frequencyDeselected();
    });
}

void FrequencyListPanel::setDemodulationAvailable(bool available)
{
    m_demodAvailable = available;
    updateTable();
}

double FrequencyListPanel::thresholdDb() const
{
    return m_thresholdSlider->value();
}

void FrequencyListPanel::setThresholdDb(double db)
{
    m_thresholdSlider->setValue(static_cast<int>(db));
}

bool FrequencyListPanel::autoRefresh() const
{
    return m_autoRefreshBox->isChecked();
}

void FrequencyListPanel::setAutoRefresh(bool on)
{
    m_autoRefreshBox->setChecked(on);
}

void FrequencyListPanel::onSweepReceived(const SweepData& sweep)
{
    m_lastSweep = sweep;
    if (m_autoRefreshBox->isChecked())
        detectNow(sweep);
}

void FrequencyListPanel::detectNow(const SweepData& sweep)
{
    m_signals = m_detector.detect(sweep);
    updateTable();
}

void FrequencyListPanel::onDetectClicked()
{
    if (!m_lastSweep.isEmpty())
        detectNow(m_lastSweep);
}

void FrequencyListPanel::onThresholdChanged(int value)
{
    m_detector.setThresholdDb(value);
    m_thresholdLabel->setText(tr("%1 dB").arg(value));

    // Re-detect with new threshold if we have data
    if (!m_lastSweep.isEmpty() && m_autoRefreshBox->isChecked())
        detectNow(m_lastSweep);
}

void FrequencyListPanel::onListenClicked(int row)
{
    if (row < 0 || row >= static_cast<int>(m_signals.size()))
        return;

    if (m_listeningRow == row) {
        // Toggle off
        m_listeningRow = -1;
        m_stopBtn->setEnabled(false);
        updateTable();
        emit stopListenRequested();
        return;
    }

    m_listeningRow = row;
    m_stopBtn->setEnabled(true);
    updateTable();
    emit listenRequested(m_signals[row].centerFreqHz);
}

void FrequencyListPanel::updateTable()
{
    m_table->setRowCount(static_cast<int>(m_signals.size()));

    for (int i = 0; i < static_cast<int>(m_signals.size()); ++i) {
        const auto& sig = m_signals[i];

        auto* freqItem = new QTableWidgetItem(
            QString("%1 MHz").arg(sig.centerFreqHz / 1e6, 0, 'f', 3));
        freqItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_table->setItem(i, 0, freqItem);

        auto* ampItem = new QTableWidgetItem(
            QString("%1 dBm").arg(sig.peakAmplitudeDbm, 0, 'f', 1));
        ampItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_table->setItem(i, 1, ampItem);

        auto* bwItem = new QTableWidgetItem(
            QString("%1 kHz").arg(sig.bandwidthHz / 1e3, 0, 'f', 1));
        bwItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_table->setItem(i, 2, bwItem);

        auto* listenBtn = new QPushButton(i == m_listeningRow ? tr("■ Stop") : tr("▶ Listen"));
        listenBtn->setEnabled(m_demodAvailable);
        if (i == m_listeningRow) {
            listenBtn->setStyleSheet("QPushButton { color: #00ff00; font-weight: bold; }");
        }
        connect(listenBtn, &QPushButton::clicked, this, [this, i]() {
            onListenClicked(i);
        });
        m_table->setCellWidget(i, 3, listenBtn);

        // Highlight listening row
        QColor rowColor = (i == m_listeningRow) ? QColor(0, 80, 0) : QColor();
        for (int col = 0; col < 3; ++col) {
            if (auto* item = m_table->item(i, col)) {
                if (rowColor.isValid())
                    item->setBackground(rowColor);
            }
        }
    }
}
