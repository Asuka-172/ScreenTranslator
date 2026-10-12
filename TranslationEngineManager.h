#ifndef TRANSLATIONENGINEMANAGER_H
#define TRANSLATIONENGINEMANAGER_H

#include <QObject>
#include <QHash>
#include <QStringList>

class ITranslator;

// 翻译引擎管理器：注册/切换引擎、翻译缓存、在线失败自动回退到离线、测试翻译。
//
// 并发模型：每个请求分配一个自增 requestId（令牌）。引擎把令牌随
// finished/error 信号带回，管理器按令牌路由结果，因此可以同时有多个
// 在途请求（文档翻译多路流水线）而互不干扰。
// 失败处理顺序：可重试错误（限流/网络/服务端）→ 指数退避重试 N 次 →
// 回退离线一次 → 最终报错。另支持按配置的请求最小间隔限流。
class TranslationEngineManager : public QObject
{
    Q_OBJECT
public:
    explicit TranslationEngineManager(QObject* parent = nullptr);

    void registerEngine(const QString& id, ITranslator* engine);
    void setCurrentEngine(const QString& id);
    QString currentEngine() const;
    QStringList engineIds() const;
    ITranslator* engine(const QString& id) const;

    // 单发路径（屏幕翻译）：内部分配令牌，结果按 finished/error（带令牌）返回。
    void translate(const QString& text, const QString& sourceLang, const QString& targetLang);
    // 并发路径（文档翻译）：返回令牌，调用方可据令牌与结果对应。
    quint64 translateAsync(const QString& text, const QString& sourceLang, const QString& targetLang);
    // 取消某令牌对应的请求（在途网络请求仍会返回，但其结果会被忽略；重试定时器随之失效）。
    void cancelRequest(quint64 requestId);

    void testTranslate(const QString& engineId, const QString& text,
                       const QString& sourceLang, const QString& targetLang);

signals:
    void finished(const QString& translatedText, const QString& detectedLang, quint64 requestId);
    void error(const QString& message, quint64 requestId);
    void currentChanged(const QString& id);
    void testResult(const QString& engineId, bool ok, const QString& message);

private:
    struct PendingReq {
        QString text, src, tgt, cacheKey;
        QString engineId;
        int retryCount = 0;
        bool fallbackTried = false;
    };

    void issueRequest(ITranslator* engine, quint64 requestId);  // 限流后调用引擎
    void handleFinished(ITranslator* e, quint64 requestId, const QString& text, const QString& lang);
    void handleError(ITranslator* e, quint64 requestId, const QString& message);
    static bool isRetryableError(const QString& message);

    QString cacheKey(const QString& engineId, const QString& src,
                     const QString& tgt, const QString& text) const;
    bool cacheLookup(const QString& key, QString& lang, QString& text);
    void cachePut(const QString& key, const QString& lang, const QString& text);

    QHash<QString, ITranslator*> m_engines;
    QStringList m_order;
    QString m_currentId;

    QHash<quint64, PendingReq> m_pending;         // 令牌 → 请求状态
    QHash<quint64, ITranslator*> m_activeByToken; // 令牌 → 当前正在使用的引擎（回退会切换）
    quint64 m_nextToken = 1;
    qint64 m_nextSlotMs = 0;                      // 限流：下一允许发请求的时刻（毫秒）

    QHash<QString, QString> m_cache;
    QStringList m_cacheOrder;
};

#endif
