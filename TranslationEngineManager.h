#ifndef TRANSLATIONENGINEMANAGER_H
#define TRANSLATIONENGINEMANAGER_H

#include <QObject>
#include <QHash>
#include <QStringList>

class ITranslator;

// 翻译引擎管理器：注册/切换引擎、翻译缓存、在线失败自动回退到离线、测试翻译。
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

    void translate(const QString& text, const QString& sourceLang, const QString& targetLang);
    void testTranslate(const QString& engineId, const QString& text,
                       const QString& sourceLang, const QString& targetLang);

signals:
    void finished(const QString& translatedText, const QString& detectedLang);
    void error(const QString& message);
    void currentChanged(const QString& id);
    void testResult(const QString& engineId, bool ok, const QString& message);

private:
    void handleFinished(ITranslator* e, const QString& text, const QString& lang);
    void handleError(ITranslator* e, const QString& message);

    QString cacheKey(const QString& engineId, const QString& src,
                     const QString& tgt, const QString& text) const;
    bool cacheLookup(const QString& key, QString& lang, QString& text);
    void cachePut(const QString& key, const QString& lang, const QString& text);

    QHash<QString, ITranslator*> m_engines;
    QStringList m_order;
    QString m_currentId;

    ITranslator* m_activeEngine = nullptr;
    QString m_pendingText, m_pendingSrc, m_pendingTgt, m_pendingCacheKey;
    bool m_fallbackTried = false;

    QHash<QString, QString> m_cache;
    QStringList m_cacheOrder;
};

#endif
