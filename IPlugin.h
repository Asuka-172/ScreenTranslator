#ifndef IPLUGIN_H
#define IPLUGIN_H

#include <QString>

// 插件基础接口。插件以动态库（.dll）形式提供，导出 createPlugin 工厂函数。
class IPlugin
{
public:
    virtual ~IPlugin() {}

    virtual QString name() const = 0;
    virtual QString version() const = 0;
    virtual QString type() const = 0;   // 目前支持 "translator"
    virtual bool init() = 0;
    virtual void shutdown() = 0;
};

// 翻译插件：同步接口，由宿主在后台线程调用（避免阻塞 UI）。
class ITranslatorPlugin : public IPlugin
{
public:
    QString type() const override { return QStringLiteral("translator"); }

    // 成功返回译文；失败返回空串并填充 error。
    virtual QString translate(const QString& text, const QString& sourceLang,
                              const QString& targetLang, QString& error) = 0;
};

// 每个插件必须导出的工厂函数。
extern "C" IPlugin* createPlugin();

#endif
