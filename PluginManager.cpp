#include "PluginManager.h"

#include <QLibrary>
#include <QDir>

namespace {

using CreateFn = IPlugin* (*)();

} // namespace

PluginManager::PluginManager(QObject* parent)
    : QObject(parent)
{
}

PluginManager::~PluginManager()
{
    unloadAll();
}

void PluginManager::loadPlugins(const QString& dir)
{
    QDir d(dir);
    if (!d.exists())
        return;

    const QStringList files = d.entryList(QStringList() << "*.dll", QDir::Files);
    for (const QString& file : files) {
        const QString path = d.absoluteFilePath(file);

        auto* library = new QLibrary(path);
        if (!library->load()) {
            emit pluginLoadFailed(path, library->errorString());
            delete library;
            continue;
        }

        auto createFn = reinterpret_cast<CreateFn>(library->resolve("createPlugin"));
        if (!createFn) {
            emit pluginLoadFailed(path, QStringLiteral("未找到 createPlugin 符号"));
            library->unload();
            delete library;
            continue;
        }

        IPlugin* plugin = createFn();
        if (!plugin) {
            emit pluginLoadFailed(path, QStringLiteral("插件创建失败"));
            library->unload();
            delete library;
            continue;
        }

        plugin->init();
        m_plugins.append({library, plugin});
        emit pluginLoaded(plugin->name());
    }
}

void PluginManager::unloadAll()
{
    for (LoadedPlugin& p : m_plugins) {
        if (p.instance)
            p.instance->shutdown();
        if (p.library) {
            p.library->unload();
            delete p.library;
        }
    }
    m_plugins.clear();
}

QList<IPlugin*> PluginManager::plugins() const
{
    QList<IPlugin*> result;
    for (const LoadedPlugin& p : m_plugins)
        result.append(p.instance);
    return result;
}

QList<ITranslatorPlugin*> PluginManager::translatorPlugins() const
{
    QList<ITranslatorPlugin*> result;
    for (const LoadedPlugin& p : m_plugins) {
        if (p.instance && p.instance->type() == "translator")
            result.append(static_cast<ITranslatorPlugin*>(p.instance));
    }
    return result;
}
