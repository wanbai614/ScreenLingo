#include <QtWidgets/QApplication>
#include <QtWidgets/QMessageBox>
#include <QtCore/QSharedMemory>
#include <QtCore/QFile>
#include <QtCore/QTextStream>
#include <QtCore/QDateTime>
#include <QtCore/QStandardPaths>
#include "app/Application.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("ScreenLingo");
    app.setOrganizationName("ScreenLingo");
    app.setQuitOnLastWindowClosed(false);

    // Redirect qDebug/qWarning to our log file
    static QFile logFile(
        QStandardPaths::writableLocation(QStandardPaths::TempLocation)
        + "/screenlingo_debug.log");
    logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
    qInstallMessageHandler([](QtMsgType, const QMessageLogContext&, const QString& msg) {
        QTextStream ts(&logFile);
        ts << QDateTime::currentDateTime().toString("hh:mm:ss.zzz")
           << "  [Qt] " << msg << "\n";
        ts.flush();
    });

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
