#ifndef PLUGINMANAGER_H
#define PLUGINMANAGER_H

#include <QObject>
#include <QList>
#include <QString>

#include "IPlugin.h"

class QLibrary;

// 通过 QLibrary 动态加载 .dll 插件。
class PluginManager : public QObject
{
    Q_OBJECT
public:
    explicit PluginManager(QObject* parent = nullptr);
    ~PluginManager() override;

    void loadPlugins(const QString& dir);
    void unloadAll();

    QList<IPlugin*> plugins() const;
    QList<ITranslatorPlugin*> translatorPlugins() const;

signals:
    void pluginLoaded(const QString& name);
    void pluginLoadFailed(const QString& path, const QString& error);

private:
    struct LoadedPlugin {
        QLibrary* library = nullptr;
        IPlugin* instance = nullptr;
    };
    QList<LoadedPlugin> m_plugins;
};

#endif
