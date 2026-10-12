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
    setDefault("baiduAppId", "");
    setDefault("baiduSecret", "");
    setDefault("fallbackEnabled", false);
    setDefault("cacheEnabled", true);
    setDefault("cacheMaxEntries", 500);
    setDefault("docChunkMaxBytes", 6000);
    setDefault("docRetryMax", 3);
    setDefault("docRetryBaseMs", 1000);
    setDefault("docMinIntervalMs", 0);
    setDefault("docConcurrency", 3);
    setDefault("resumeEnabled", true);
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

QString AppConfig::baiduAppId() const { return m_settings.value("baiduAppId", "").toString(); }
void AppConfig::setBaiduAppId(const QString& id) { setValue("baiduAppId", id); }

QString AppConfig::baiduSecret() const { return m_settings.value("baiduSecret", "").toString(); }
void AppConfig::setBaiduSecret(const QString& s) { setValue("baiduSecret", s); }

bool AppConfig::fallbackEnabled() const { return m_settings.value("fallbackEnabled", false).toBool(); }
void AppConfig::setFallbackEnabled(bool e) { setValue("fallbackEnabled", e); }

bool AppConfig::cacheEnabled() const { return m_settings.value("cacheEnabled", true).toBool(); }
void AppConfig::setCacheEnabled(bool e) { setValue("cacheEnabled", e); }

int AppConfig::cacheMaxEntries() const { return m_settings.value("cacheMaxEntries", 500).toInt(); }
void AppConfig::setCacheMaxEntries(int n) { setValue("cacheMaxEntries", n); }

int AppConfig::docChunkMaxBytes() const { return m_settings.value("docChunkMaxBytes", 6000).toInt(); }
void AppConfig::setDocChunkMaxBytes(int n) { setValue("docChunkMaxBytes", n); }

int AppConfig::docRetryMax() const { return m_settings.value("docRetryMax", 3).toInt(); }
void AppConfig::setDocRetryMax(int n) { setValue("docRetryMax", n); }

int AppConfig::docRetryBaseMs() const { return m_settings.value("docRetryBaseMs", 1000).toInt(); }
void AppConfig::setDocRetryBaseMs(int n) { setValue("docRetryBaseMs", n); }

int AppConfig::docMinIntervalMs() const { return m_settings.value("docMinIntervalMs", 0).toInt(); }
void AppConfig::setDocMinIntervalMs(int n) { setValue("docMinIntervalMs", n); }

int AppConfig::docConcurrency() const { return m_settings.value("docConcurrency", 3).toInt(); }
void AppConfig::setDocConcurrency(int n) { setValue("docConcurrency", n); }

bool AppConfig::resumeEnabled() const { return m_settings.value("resumeEnabled", true).toBool(); }
void AppConfig::setResumeEnabled(bool e) { setValue("resumeEnabled", e); }

QString AppConfig::hotkey(const QString& action) const
{
    return m_settings.value("hotkey_" + action, QString()).toString();
}

void AppConfig::setHotkey(const QString& action, const QString& seq)
{
    setValue("hotkey_" + action, seq);
}
