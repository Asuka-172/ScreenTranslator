#ifndef APPCONFIG_H
#define APPCONFIG_H

#include <QObject>
#include <QSettings>
#include <QVariant>
#include <QString>

// 应用配置单例，基于 QSettings（ini 格式）持久化，修改即时发出 changed 信号。
class AppConfig : public QObject
{
    Q_OBJECT
public:
    static AppConfig& instance();

    // 通用访问
    QVariant value(const QString& key, const QVariant& def = QVariant()) const;
    void setValue(const QString& key, const QVariant& v);

    // 外观
    QString theme() const;                 // "dark" | "light" | "highcontrast"
    void setTheme(const QString& t);
    QString fontFamily() const;
    void setFontFamily(const QString& f);
    int fontSize() const;                  // 12-20
    void setFontSize(int s);
    int windowOpacity() const;             // 10-100
    void setWindowOpacity(int o);

    // TTS
    bool ttsEnabled() const;
    void setTtsEnabled(bool e);
    double ttsRate() const;                // 0.5 - 2.0
    void setTtsRate(double r);
    int ttsVolume() const;                 // 0-100
    void setTtsVolume(int v);

    // 翻译与导出
    QString translatorEngine() const;      // "google" 或插件 id
    void setTranslatorEngine(const QString& e);
    QString exportFormat() const;          // "txt" | "docx" | "md" | "csv"
    void setExportFormat(const QString& f);

    // 自定义在线引擎
    QString customEngineUrl() const;       // 含 {text} {from} {to} {key} 占位符
    void setCustomEngineUrl(const QString& u);
    QString customEngineApiKey() const;
    void setCustomEngineApiKey(const QString& k);
    QString customEngineKeyHeader() const; // 空=密钥放 URL {key}；非空=作为请求头名
    void setCustomEngineKeyHeader(const QString& h);
    QString customEngineResultPath() const; // JSON 结果路径，空=整个响应体即译文
    void setCustomEngineResultPath(const QString& p);

    // 离线引擎
    bool offlineEnabled() const;
    void setOfflineEnabled(bool e);
    QString offlineModelPath() const;      // 空=运行时解析为 <appdir>/models
    void setOfflineModelPath(const QString& p);
    QString offlineGlossaryPath() const;   // 空=运行时解析为 <appdir>/models/glossary.txt
    void setOfflineGlossaryPath(const QString& p);

    // 百度翻译插件
    QString baiduAppId() const;
    void setBaiduAppId(const QString& id);
    QString baiduSecret() const;
    void setBaiduSecret(const QString& s);

    // 回退与缓存
    bool fallbackEnabled() const;
    void setFallbackEnabled(bool e);
    bool cacheEnabled() const;
    void setCacheEnabled(bool e);
    int cacheMaxEntries() const;
    void setCacheMaxEntries(int n);

    // 文档翻译（长文）
    int docChunkMaxBytes() const;          // 单请求分块字节上限，默认 6000
    void setDocChunkMaxBytes(int n);
    int docRetryMax() const;               // 可重试错误的最大重试次数，默认 3
    void setDocRetryMax(int n);
    int docRetryBaseMs() const;            // 重试退避基础间隔 ms，默认 1000
    void setDocRetryBaseMs(int n);
    int docMinIntervalMs() const;          // 引擎请求最小间隔 ms，0=不限（百度标准版建议 1100）
    void setDocMinIntervalMs(int n);
    int docConcurrency() const;            // 文档翻译并发路数，默认 3
    void setDocConcurrency(int n);
    bool resumeEnabled() const;            // 启用断点续传
    void setResumeEnabled(bool e);

    // 全局热键（action -> QKeySequence 字符串）
    QString hotkey(const QString& action) const;
    void setHotkey(const QString& action, const QString& seq);

signals:
    void changed(const QString& key);

private:
    AppConfig();
    void ensureDefaults();

    QSettings m_settings;
};

#endif
