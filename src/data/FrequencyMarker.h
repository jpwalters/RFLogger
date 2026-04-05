#pragma once

#include <QColor>
#include <QString>

struct FrequencyMarker
{
    double frequencyHz = 0.0;
    QString label;
    QColor color = QColor(255, 255, 0);
    bool visible = true;
};
