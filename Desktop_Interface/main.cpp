#include "mainwindow.h"
#include <QApplication>
#include "webserver.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

#ifdef PLATFORM_ANDROID
    //Android default font sizes are just plain ridiculous
    QFont font = qApp->font();
    font.setPointSize(12);
    qApp->setFont(font);
#endif

    init_webserver();

    MainWindow w;
    link_webserver(&w);

    w.show();

    return a.exec();
}
