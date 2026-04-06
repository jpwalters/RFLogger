#pragma once

#include <QSettings>
#include <QVariant>

class QWidget;

class SettingsManager
{
public:
    static void setValue(const QString& key, const QVariant& value);
    static QVariant value(const QString& key, const QVariant& defaultValue = {});
    static bool contains(const QString& key);

    static void saveWindowGeometry(QWidget* window);
    static void loadWindowGeometry(QWidget* window);

private:
    static QSettings& settings();
};
