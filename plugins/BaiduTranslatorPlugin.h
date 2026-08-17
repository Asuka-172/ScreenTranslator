#ifndef BAIDUTRANSLATORPLUGIN_H
#define BAIDUTRANSLATORPLUGIN_H

#include "IPlugin.h"

// 百度翻译示例插件。需通过环境变量 BAIDU_APPID / BAIDU_SECRET 提供密钥。
class BaiduTranslatorPlugin : public ITranslatorPlugin
{
public:
    QString name() const override;
    QString version() const override;
    bool init() override;
    void shutdown() override;
    QString translate(const QString& text, const QString& sourceLang,
                      const QString& targetLang, QString& error) override;
};

#endif
