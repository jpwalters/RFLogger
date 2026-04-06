#pragma once

#include <QApplication>

namespace CrashHandler {

/// Install global crash handlers. Call once at startup, before QApplication.
void install();

/// Handle a caught C++ exception — show dialog, write log, exit.
void handleException(const char* what, const char* typeName);

}

/// QApplication subclass that catches C++ exceptions escaping Qt event dispatch.
class SafeApplication : public QApplication
{
public:
    using QApplication::QApplication;
    bool notify(QObject* receiver, QEvent* event) override;
};
