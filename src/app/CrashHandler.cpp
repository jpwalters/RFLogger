#include "CrashHandler.h"

#include <cstdio>
#include <cstdlib>
#include <csignal>
#include <exception>
#include <string>
#include <typeinfo>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QMessageBox>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTextStream>
#include <QFile>

namespace {

// Write a crash log file and return the path
QString writeCrashLog(const QString& errorType, const QString& detail)
{
    QString logDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(logDir);
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    QString logPath = logDir + "/crash_" + timestamp + ".log";

    QFile file(logPath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out << "RF Logger Crash Report\n";
        out << "======================\n\n";
        out << "Time:    " << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n";
        out << "Version: " << QCoreApplication::applicationVersion() << "\n";
        out << "OS:      " << QSysInfo::prettyProductName() << "\n";
        out << "Arch:    " << QSysInfo::currentCpuArchitecture() << "\n\n";
        out << "Error Type: " << errorType << "\n";
        out << "Detail:     " << detail << "\n\n";

#ifdef _WIN32
        // Capture basic stack info via CaptureStackBackTrace
        void* stack[64];
        USHORT frames = CaptureStackBackTrace(0, 64, stack, nullptr);
        out << "Stack addresses (" << frames << " frames):\n";
        for (USHORT i = 0; i < frames; ++i) {
            out << QString("  [%1] 0x%2\n")
                       .arg(i, 2, 10, QChar('0'))
                       .arg(reinterpret_cast<quintptr>(stack[i]), 16, 16, QChar('0'));
        }
#endif
        file.close();
    }
    return logPath;
}

// Show error using Win32 MessageBox (safe even if Qt event loop is broken)
void showCrashMessage(const QString& errorType, const QString& detail, const QString& logPath)
{
    QString message =
        QString("RF Logger encountered an unexpected error and needs to close.\n\n"
                "What happened:\n"
                "  %1\n\n"
                "Technical detail:\n"
                "  %2\n\n")
            .arg(errorType, detail);

    if (!logPath.isEmpty()) {
        message += QString("A crash log has been saved to:\n  %1\n\n").arg(QDir::toNativeSeparators(logPath));
    }

    message += "Please restart the application. If this keeps happening,\n"
               "report the issue with the crash log attached.";

#ifdef _WIN32
    MessageBoxW(nullptr,
                reinterpret_cast<LPCWSTR>(message.utf16()),
                L"RF Logger — Unexpected Error",
                MB_OK | MB_ICONERROR | MB_TOPMOST | MB_SETFOREGROUND);
#else
    // Fallback: try QMessageBox if the event loop is still alive
    if (QApplication::instance()) {
        QMessageBox::critical(nullptr, "RF Logger — Unexpected Error", message);
    } else {
        fprintf(stderr, "%s\n", message.toUtf8().constData());
    }
#endif
}

void handleCrash(const QString& errorType, const QString& detail)
{
    QString logPath = writeCrashLog(errorType, detail);
    showCrashMessage(errorType, detail, logPath);
    _exit(1);
}

// ── C++ terminate handler ────────────────────────────────────────────────────
void terminateHandler()
{
    QString detail;
    try {
        auto eptr = std::current_exception();
        if (eptr)
            std::rethrow_exception(eptr);
    } catch (const std::exception& e) {
        detail = QString("C++ exception: %1\nType: %2")
                     .arg(e.what(), typeid(e).name());
    } catch (...) {
        detail = "Unknown C++ exception (not derived from std::exception)";
    }

    if (detail.isEmpty())
        detail = "std::terminate() called without an active exception";

    handleCrash("Unhandled C++ Exception", detail);
}

// ── Signal handler (SIGSEGV, SIGABRT, SIGFPE) ───────────────────────────────
void signalHandler(int sig)
{
    const char* name = "Unknown signal";
    switch (sig) {
    case SIGSEGV: name = "SIGSEGV (Segmentation fault — access violation)"; break;
    case SIGABRT: name = "SIGABRT (Abnormal termination)"; break;
    case SIGFPE:  name = "SIGFPE (Floating point exception)"; break;
    case SIGILL:  name = "SIGILL (Illegal instruction)"; break;
    }
    handleCrash("Fatal Signal", QString(name));
}

#ifdef _WIN32
// ── Windows SEH (Structured Exception Handling) ──────────────────────────────
QString sehCodeToString(DWORD code)
{
    switch (code) {
    case EXCEPTION_ACCESS_VIOLATION:      return "Access Violation (reading/writing invalid memory)";
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: return "Array Bounds Exceeded";
    case EXCEPTION_DATATYPE_MISALIGNMENT: return "Data Type Misalignment";
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:   return "Floating Point Divide by Zero";
    case EXCEPTION_FLT_OVERFLOW:          return "Floating Point Overflow";
    case EXCEPTION_ILLEGAL_INSTRUCTION:   return "Illegal Instruction";
    case EXCEPTION_IN_PAGE_ERROR:         return "Page Error (device disconnected or memory-mapped I/O failure)";
    case EXCEPTION_INT_DIVIDE_BY_ZERO:    return "Integer Divide by Zero";
    case EXCEPTION_INT_OVERFLOW:          return "Integer Overflow";
    case EXCEPTION_STACK_OVERFLOW:        return "Stack Overflow";
    case EXCEPTION_GUARD_PAGE:            return "Guard Page Violation";
    default:
        return QString("Exception code 0x%1").arg(code, 8, 16, QChar('0'));
    }
}

LONG WINAPI sehHandler(EXCEPTION_POINTERS* info)
{
    if (!info || !info->ExceptionRecord)
        return EXCEPTION_CONTINUE_SEARCH;

    DWORD code = info->ExceptionRecord->ExceptionCode;

    // Skip non-fatal exceptions (C++ exceptions use this code internally)
    if (code == 0xE06D7363) // MSVC C++ exception
        return EXCEPTION_CONTINUE_SEARCH;

    QString detail = sehCodeToString(code);

    // For access violations, include the faulting address
    if (code == EXCEPTION_ACCESS_VIOLATION && info->ExceptionRecord->NumberParameters >= 2) {
        auto addr = info->ExceptionRecord->ExceptionInformation[1];
        auto rw = info->ExceptionRecord->ExceptionInformation[0]; // 0=read, 1=write
        detail += QString("\n%1 address: 0x%2")
                      .arg(rw == 0 ? "Reading from" : "Writing to")
                      .arg(addr, 16, 16, QChar('0'));
    }

    // Include instruction pointer
    if (info->ContextRecord) {
#ifdef _M_X64
        detail += QString("\nInstruction pointer: 0x%1")
                      .arg(info->ContextRecord->Rip, 16, 16, QChar('0'));
#elif defined(_M_IX86)
        detail += QString("\nInstruction pointer: 0x%1")
                      .arg(info->ContextRecord->Eip, 8, 16, QChar('0'));
#endif
    }

    handleCrash("Windows Exception", detail);
    return EXCEPTION_EXECUTE_HANDLER; // Not reached due to _exit()
}
#endif

// ── Qt message handler (catches qFatal) ──────────────────────────────────────
QtMessageHandler g_previousHandler = nullptr;

void qtMessageHandler(QtMsgType type, const QMessageLogContext& context, const QString& msg)
{
    if (type == QtFatalMsg) {
        QString detail = msg;
        if (context.file)
            detail += QString("\nFile: %1:%2").arg(context.file).arg(context.line);
        if (context.function)
            detail += QString("\nFunction: %1").arg(context.function);
        handleCrash("Qt Fatal Error", detail);
    }

    // Forward non-fatal messages to the previous handler
    if (g_previousHandler)
        g_previousHandler(type, context, msg);
}

} // anonymous namespace

namespace CrashHandler {

void install()
{
    // C++ unhandled exception
    std::set_terminate(terminateHandler);

    // POSIX signals
    std::signal(SIGSEGV, signalHandler);
    std::signal(SIGABRT, signalHandler);
    std::signal(SIGFPE, signalHandler);
    std::signal(SIGILL, signalHandler);

#ifdef _WIN32
    // Windows Structured Exception Handling — catches access violations, etc.
    SetUnhandledExceptionFilter(sehHandler);
#endif

    // Qt fatal message handler
    g_previousHandler = qInstallMessageHandler(qtMessageHandler);
}

void handleException(const char* what, const char* typeName)
{
    QString detail = QString("C++ exception: %1").arg(what ? what : "(no message)");
    if (typeName)
        detail += QString("\nType: %1").arg(typeName);
    handleCrash("Unhandled C++ Exception", detail);
}

} // namespace CrashHandler

// ── SafeApplication: catches C++ exceptions escaping Qt event dispatch ───────
bool SafeApplication::notify(QObject* receiver, QEvent* event)
{
    try {
        return QApplication::notify(receiver, event);
    } catch (const std::exception& e) {
        CrashHandler::handleException(e.what(), typeid(e).name());
    } catch (...) {
        CrashHandler::handleException("Unknown exception (not derived from std::exception)", nullptr);
    }
    return false; // Not reached
}
