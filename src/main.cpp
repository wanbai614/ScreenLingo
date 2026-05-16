#include <QtWidgets/QApplication>

int main(int argc, char* argv[])
{
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    QApplication app(argc, argv);
    app.setApplicationName("ScreenLingo");
    app.setOrganizationName("ScreenLingo");
    app.setQuitOnLastWindowClosed(false);

    return app.exec();
}
