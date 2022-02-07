#include "mainwindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QStringList paths = QCoreApplication::libraryPaths();
    paths.append(".");
    paths.append("imageformats");
    paths.append("platforms");
    QCoreApplication::setLibraryPaths(paths);
    QApplication a(argc, argv);
    MainWindow w;

    return a.exec();
}
