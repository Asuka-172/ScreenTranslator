#include "AppConfig.h"
#include <QStandardPaths>
#include <QDir>

namespace {

QString configFilePath()
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (!dir.isEmpty())
        QDir().mkpath(dir);
    return dir.isEmpty()
        ? QStringLiteral("config.ini")
        : dir + QStringLiteral("/config.ini");
}

} // namespace

AppConfig& AppConfig::instance()
{
    static AppConfig inst;
    return inst;
}

AppConfig::AppConfig()
    : m_settings(configFilePath(), QSettings::IniFormat)
{
    ensureDefaults();
}

void AppConfig::ensureDefaults()
{
    auto setDefault = [this](const char* key, const QVariant& v) {
        if (!m_settings.contains(QLatin1String(key)))
            m_settings.setValue(QLatin1String(key), v);
    };
    setDefault("theme", "dark");
    setDefault("fontFamily", "Microsoft YaHei");
    setDefault("fontSize", 14);
    setDefault("windowOpacity", 85);
    setDefault("ttsEnabled", false);
    setDefault("ttsRate", 1.0);
    setDefault("ttsVolume", 80);
    setDefault("translatorEngine", "google");
    setDefault("exportFormat", "txt");
    setDefault("customEngineUrl", "");
    setDefault("customEngineApiKey", "");
    setDefault("customEngineKeyHeader", "");
    setDefault("customEngineResultPath", "");
    setDefault("offlineEnabled", false);
    setDefault("offlineModelPath", "");
    setDefault("offlineGlossaryPath", "");
    setDefault("fallbackEnabled", false);
    setDefault("cacheEnabled", true);
    setDefault("cacheMaxEntries", 500);
}

QVariant AppConfig::value(const QString& key, const QVariant& def) const
{
    return m_settings.value(key, def);
}

void AppConfig::setValue(const QString& key, const QVariant& v)
{
    if (m_settings.value(key) == v)
        return;
    m_settings.setValue(key, v);
    m_settings.sync();
    emit changed(key);
}

QString AppConfig::theme() const { return m_settings.value("theme", "dark").toString(); }
void AppConfig::setTheme(const QString& t) { setValue("theme", t); }

QString AppConfig::fontFamily() const { return m_settings.value("fontFamily", "Microsoft YaHei").toString(); }
void AppConfig::setFontFamily(const QString& f) { setValue("fontFamily", f); }

int AppConfig::fontSize() const { return m_settings.value("fontSize", 14).toInt(); }
void AppConfig::setFontSize(int s) { setValue("fontSize", s); }

int AppConfig::windowOpacity() const { return m_settings.value("windowOpacity", 85).toInt(); }
void AppConfig::setWindowOpacity(int o) { setValue("windowOpacity", o); }

bool AppConfig::ttsEnabled() const { return m_settings.value("ttsEnabled", false).toBool(); }
void AppConfig::setTtsEnabled(bool e) { setValue("ttsEnabled", e); }

double AppConfig::ttsRate() const { return m_settings.value("ttsRate", 1.0).toDouble(); }
void AppConfig::setTtsRate(double r) { setValue("ttsRate", r); }

int AppConfig::ttsVolume() const { return m_settings.value("ttsVolume", 80).toInt(); }
void AppConfig::setTtsVolume(int v) { setValue("ttsVolume", v); }

QString AppConfig::translatorEngine() const { return m_settings.value("translatorEngine", "google").toString(); }
void AppConfig::setTranslatorEngine(const QString& e) { setValue("translatorEngine", e); }

QString AppConfig::exportFormat() const { return m_settings.value("exportFormat", "txt").toString(); }
void AppConfig::setExportFormat(const QString& f) { setValue("exportFormat", f); }

QString AppConfig::customEngineUrl() const { return m_settings.value("customEngineUrl", "").toString(); }
void AppConfig::setCustomEngineUrl(const QString& u) { setValue("customEngineUrl", u); }

QString AppConfig::customEngineApiKey() const { return m_settings.value("customEngineApiKey", "").toString(); }
void AppConfig::setCustomEngineApiKey(const QString& k) { setValue("customEngineApiKey", k); }

QString AppConfig::customEngineKeyHeader() const { return m_settings.value("customEngineKeyHeader", "").toString(); }
void AppConfig::setCustomEngineKeyHeader(const QString& h) { setValue("customEngineKeyHeader", h); }

QString AppConfig::customEngineResultPath() const { return m_settings.value("customEngineResultPath", "").toString(); }
void AppConfig::setCustomEngineResultPath(const QString& p) { setValue("customEngineResultPath", p); }

bool AppConfig::offlineEnabled() const { return m_settings.value("offlineEnabled", false).toBool(); }
void AppConfig::setOfflineEnabled(bool e) { setValue("offlineEnabled", e); }

QString AppConfig::offlineModelPath() const { return m_settings.value("offlineModelPath", "").toString(); }
void AppConfig::setOfflineModelPath(const QString& p) { setValue("offlineModelPath", p); }

QString AppConfig::offlineGlossaryPath() const { return m_settings.value("offlineGlossaryPath", "").toString(); }
void AppConfig::setOfflineGlossaryPath(const QString& p) { setValue("offlineGlossaryPath", p); }

bool AppConfig::fallbackEnabled() const { return m_settings.value("fallbackEnabled", false).toBool(); }
void AppConfig::setFallbackEnabled(bool e) { setValue("fallbackEnabled", e); }

bool AppConfig::cacheEnabled() const { return m_settings.value("cacheEnabled", true).toBool(); }
void AppConfig::setCacheEnabled(bool e) { setValue("cacheEnabled", e); }

int AppConfig::cacheMaxEntries() const { return m_settings.value("cacheMaxEntries", 500).toInt(); }
void AppConfig::setCacheMaxEntries(int n) { setValue("cacheMaxEntries", n); }

QString AppConfig::hotkey(const QString& action) const
{
    return m_settings.value("hotkey_" + action, QString()).toString();
}

void AppConfig::setHotkey(const QString& action, const QString& seq)
{
    setValue("hotkey_" + action, seq);
}
