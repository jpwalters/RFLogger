#pragma once

#include "data/FrequencyMarker.h"

#include <QWidget>
#include <QListWidget>
#include <QPushButton>
#include <QVector>

class MarkerPanel : public QWidget
{
    Q_OBJECT

public:
    explicit MarkerPanel(QWidget* parent = nullptr);

    const QVector<FrequencyMarker>& markers() const { return m_markers; }

public slots:
    void addMarkerAtFrequency(double freqHz);

signals:
    void markersChanged(const QVector<FrequencyMarker>& markers);

private slots:
    void onAddClicked();
    void onRemoveClicked();

private:
    void refreshList();

    QListWidget* m_listWidget;
    QPushButton* m_addBtn;
    QPushButton* m_removeBtn;

    QVector<FrequencyMarker> m_markers;
};
