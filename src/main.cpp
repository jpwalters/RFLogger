#include <QApplication>
#include <QIcon>
#include "app/MainWindow.h"
#include "app/CrashHandler.h"
#include "theme/DarkTheme.h"
#include "Version.h"

int main(int argc, char* argv[])
{
    CrashHandler::install();

    SafeApplication app(argc, argv);

    app.setApplicationName("RFLogger");
    app.setApplicationVersion(RFLOGGER_VERSION_STRING);
    app.setOrganizationName("RFLogger");
    app.setOrganizationDomain("github.com/jpwalters/RFLogger");
    app.setWindowIcon(QIcon(":/icons/rflogger.png"));

    DarkTheme::apply(app);

    MainWindow window;
    window.show();

    // Hardware-free demo/visualisation mode. Use --demo (PLUS model) or
    // --demo-basic (basic model, exercises the accumulation path).
    const QStringList args = app.arguments();
    if (args.contains("--demo-basic"))
        window.startDemoMode(/*plus=*/false);
    else if (args.contains("--demo"))
        window.startDemoMode(/*plus=*/true);

    return app.exec();
}
