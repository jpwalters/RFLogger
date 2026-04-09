#include "SettingsManager.h"

#include <QMainWindow>
#include <QWidget>

QSettings& SettingsManager::settings()
{
    static QSettings s("RFLogger", "RFLogger");
    return s;
}

void SettingsManager::setValue(const QString& key, const QVariant& value)
{
    settings().setValue(key, value);
}

QVariant SettingsManager::value(const QString& key, const QVariant& defaultValue)
{
    return settings().value(key, defaultValue);
}

bool SettingsManager::contains(const QString& key)
{
    return settings().contains(key);
}

void SettingsManager::saveWindowGeometry(QWidget* window)
{
    settings().setValue("window/geometry", window->saveGeometry());
    if (auto* mw = qobject_cast<QMainWindow*>(window))
        settings().setValue("window/state", mw->saveState());
}

void SettingsManager::loadWindowGeometry(QWidget* window)
{
    QByteArray geometry = settings().value("window/geometry").toByteArray();
    if (geometry.isEmpty() || !window->restoreGeometry(geometry))
        window->resize(1280, 800);

    if (auto* mw = qobject_cast<QMainWindow*>(window)) {
        QByteArray state = settings().value("window/state").toByteArray();
        if (!state.isEmpty())
            mw->restoreState(state);
    }
}
