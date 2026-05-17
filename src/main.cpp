#include <QtWidgets/QApplication>
#include <QtWidgets/QMessageBox>
#include <QtCore/QSharedMemory>
#include "app/Application.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("ScreenLingo");
    app.setOrganizationName("ScreenLingo");
    app.setQuitOnLastWindowClosed(false);

    // Single-instance guard
    QSharedMemory sharedMem("ScreenLingo_SingleInstance_9F8E2D1C");
    if (!sharedMem.create(1)) {
        QMessageBox::information(nullptr, "ScreenLingo",
            QStringLiteral("ScreenLingo is already running.\n"
                           "Check the system tray icon."));
        return 0;
    }

    Application screenLingo;
    if (!screenLingo.initialize()) {
        return 1;
    }

    return app.exec();
}
