#pragma once

class QApplication;
class QWidget;

class DarkTheme
{
public:
    static void apply(QApplication& app);

    // Force the native window title bar (Windows non-client area) to use the
    // dark theme. No-op on platforms other than Windows. The window must have
    // a valid native handle (i.e. be shown at least once).
    static void applyDarkTitleBar(QWidget* window);
};
