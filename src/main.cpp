#include <QApplication>
#include <QFont>
#include "ScreenTranslator.h"
#include "AppConfig.h"
#include "ThemeManager.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("ScreenTranslator");
    app.setOrganizationName("ScreenTranslator");

    // 启动时加载配置并应用主题/字体
    ThemeManager::apply(app, AppConfig::instance().theme());
    app.setFont(QFont(AppConfig::instance().fontFamily(), AppConfig::instance().fontSize()));

    ScreenTranslator translator;
    translator.show();
    return app.exec();
}
