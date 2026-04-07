#include "MarkerPanel.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QInputDialog>
#include <QColorDialog>

MarkerPanel::MarkerPanel(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);

    auto* group = new QGroupBox(tr("Frequency Markers"));
    auto* groupLayout = new QVBoxLayout(group);

    m_listWidget = new QListWidget;
    groupLayout->addWidget(m_listWidget);

    auto* btnLayout = new QHBoxLayout;
    m_addBtn = new QPushButton(tr("Add"));
    m_removeBtn = new QPushButton(tr("Remove"));
    m_removeBtn->setEnabled(false);
    btnLayout->addWidget(m_addBtn);
    btnLayout->addWidget(m_removeBtn);
    groupLayout->addLayout(btnLayout);

    layout->addWidget(group);
    layout->addStretch();

    connect(m_addBtn, &QPushButton::clicked, this, &MarkerPanel::onAddClicked);
    connect(m_removeBtn, &QPushButton::clicked, this, &MarkerPanel::onRemoveClicked);
    connect(m_listWidget, &QListWidget::currentRowChanged, this, [this](int row) {
        m_removeBtn->setEnabled(row >= 0);
    });
}

void MarkerPanel::addMarkerAtFrequency(double freqHz)
{
    bool ok;
    QString label = QInputDialog::getText(this, tr("Add Marker"),
                                          tr("Label for %.3f MHz:").arg(freqHz / 1e6),
                                          QLineEdit::Normal, {}, &ok);
    if (!ok)
        return;

    FrequencyMarker marker;
    marker.frequencyHz = freqHz;
    marker.label = label.isEmpty() ? QString("%1 MHz").arg(freqHz / 1e6, 0, 'f', 3) : label;

    // Cycle through some default colors
    static const QColor defaultColors[] = {
        QColor(255, 255, 0),   // Yellow
        QColor(255, 128, 0),   // Orange
        QColor(255, 0, 255),   // Magenta
        QColor(0, 255, 255),   // Cyan
        QColor(255, 128, 128), // Light red
    };
    marker.color = defaultColors[static_cast<int>(m_markers.size()) % 5];

    m_markers.append(marker);
    refreshList();
    emit markersChanged(m_markers);
}

void MarkerPanel::onAddClicked()
{
    bool ok;
    double freqMHz = QInputDialog::getDouble(this, tr("Add Marker"),
                                              tr("Frequency (MHz):"),
                                              600.0, 0.0, 7000.0, 3, &ok);
    if (ok)
        addMarkerAtFrequency(freqMHz * 1'000'000.0);
}

void MarkerPanel::onRemoveClicked()
{
    int row = m_listWidget->currentRow();
    if (row < 0 || row >= static_cast<int>(m_markers.size()))
        return;

    m_markers.removeAt(row);
    refreshList();
    emit markersChanged(m_markers);
}

void MarkerPanel::refreshList()
{
    m_listWidget->clear();
    for (const auto& marker : m_markers) {
        auto* item = new QListWidgetItem(
            QString("%1 — %2 MHz").arg(marker.label).arg(marker.frequencyHz / 1e6, 0, 'f', 3));

        item->setForeground(marker.color);
        m_listWidget->addItem(item);
    }
}
