#include <QApplication>
#include "ScreenTranslator.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    ScreenTranslator translator;
    translator.show();
    return app.exec();
}