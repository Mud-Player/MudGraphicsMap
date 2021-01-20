#include "mudmap.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MudMap w;
    w.show();
    return a.exec();
}
