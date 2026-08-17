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
