#include "PluginTranslator.h"

#include <future>
#include <QMetaObject>

PluginTranslator::PluginTranslator(ITranslatorPlugin* plugin, QObject* parent)
    : ITranslator(parent)
    , m_plugin(plugin)
{
}

QString PluginTranslator::name() const
{
    return m_plugin ? m_plugin->name() : QString();
}

void PluginTranslator::translate(const QString& text, const QString& sourceLang, const QString& targetLang)
{
    if (!m_plugin)
        return;

    ITranslatorPlugin* plugin = m_plugin;
    std::async(std::launch::async, [this, plugin, text, sourceLang, targetLang]() {
        QString err;
        const QString result = plugin->translate(text, sourceLang, targetLang, err);

        QMetaObject::invokeMethod(this, [this, result, err]() {
            if (result.isEmpty() && !err.isEmpty())
                emit error(err);
            else
                emit finished(result, QString());
        }, Qt::QueuedConnection);
    });
}
