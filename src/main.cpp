#include <QApplication>
#include "app/MainWindow.h"
#include "theme/DarkTheme.h"
#include "Version.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    app.setApplicationName("RFLogger");
    app.setApplicationVersion(RFLOGGER_VERSION_STRING);
    app.setOrganizationName("RFLogger");
    app.setOrganizationDomain("github.com/jpwalters/RFLogger");

    DarkTheme::apply(app);

    MainWindow window;
    window.show();

    return app.exec();
}
