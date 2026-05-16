#include <QtWidgets/QApplication>
#include "app/Application.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("ScreenLingo");
    app.setOrganizationName("ScreenLingo");
    app.setQuitOnLastWindowClosed(false);

    Application screenLingo;
    if (!screenLingo.initialize()) {
        return 1;
    }

    return app.exec();
}
