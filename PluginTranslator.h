#ifndef PLUGINTRANSLATOR_H
#define PLUGINTRANSLATOR_H

#include "ITranslator.h"
#include "IPlugin.h"

// 将同步的 ITranslatorPlugin 适配为异步的 ITranslator（后台线程调用插件）。
class PluginTranslator : public ITranslator
{
    Q_OBJECT
public:
    PluginTranslator(ITranslatorPlugin* plugin, QObject* parent = nullptr);

    QString name() const override;
    void translate(const QString& text, const QString& sourceLang, const QString& targetLang) override;

private:
    ITranslatorPlugin* m_plugin;
};

#endif
