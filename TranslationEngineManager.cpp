#include "TranslationEngineManager.h"
#include "ITranslator.h"
#include "AppConfig.h"

#include <QCryptographicHash>
#include <memory>

TranslationEngineManager::TranslationEngineManager(QObject* parent)
    : QObject(parent)
{
}

void TranslationEngineManager::registerEngine(const QString& id, ITranslator* engine)
{
    if (!engine || m_engines.contains(id))
        return;
    m_engines.insert(id, engine);
    m_order.append(id);

    connect(engine, &ITranslator::finished, this,
        [this, engine](const QString& text, const QString& lang) {
            handleFinished(engine, text, lang);
        });
    connect(engine, &ITranslator::error, this,
        [this, engine](const QString& message) {
            handleError(engine, message);
        });
}

void TranslationEngineManager::setCurrentEngine(const QString& id)
{
    if (!m_engines.contains(id) || id == m_currentId)
        return;
    m_currentId = id;
    emit currentChanged(id);
}

QString TranslationEngineManager::currentEngine() const
{
    return m_currentId;
}

QStringList TranslationEngineManager::engineIds() const
{
    return m_order;
}

ITranslator* TranslationEngineManager::engine(const QString& id) const
{
    return m_engines.value(id, nullptr);
}

void TranslationEngineManager::translate(const QString& text,
    const QString& sourceLang,
    const QString& targetLang)
{
    ITranslator* current = m_engines.value(m_currentId, nullptr);
    if (!current) {
        emit error(QStringLiteral("未选择翻译引擎"));
        return;
    }

    const QString key = cacheKey(m_currentId, sourceLang, targetLang, text);
    if (AppConfig::instance().cacheEnabled()) {
        QString lang, cached;
        if (cacheLookup(key, lang, cached)) {
            emit finished(cached, lang);
            return;
        }
    }

    m_pendingText = text;
    m_pendingSrc = sourceLang;
    m_pendingTgt = targetLang;
    m_pendingCacheKey = key;
    m_fallbackTried = false;
    m_activeEngine = current;

    current->translate(text, sourceLang, targetLang);
}

void TranslationEngineManager::handleFinished(ITranslator* e, const QString& text, const QString& lang)
{
    if (e != m_activeEngine)
        return;

    if (AppConfig::instance().cacheEnabled())
        cachePut(m_pendingCacheKey, lang, text);

    m_activeEngine = nullptr;
    emit finished(text, lang);
}

void TranslationEngineManager::handleError(ITranslator* e, const QString& message)
{
    if (e != m_activeEngine)
        return;

    // 在线引擎失败时，若启用回退且离线引擎可用，则切换到离线重试一次。
    ITranslator* offline = m_engines.value(QStringLiteral("offline"), nullptr);
    const bool canFallback = !m_fallbackTried
        && m_currentId != QStringLiteral("offline")
        && offline
        && AppConfig::instance().fallbackEnabled()
        && AppConfig::instance().offlineEnabled();

    if (canFallback) {
        m_fallbackTried = true;
        m_activeEngine = offline;
        offline->translate(m_pendingText, m_pendingSrc, m_pendingTgt);
        return;
    }

    m_activeEngine = nullptr;
    emit error(message);
}

void TranslationEngineManager::testTranslate(const QString& engineId, const QString& text,
    const QString& sourceLang, const QString& targetLang)
{
    ITranslator* e = engine(engineId);
    if (!e) {
        emit testResult(engineId, false, QStringLiteral("引擎不存在"));
        return;
    }

    auto done = std::make_shared<bool>(false);
    auto finishConn = std::make_shared<QMetaObject::Connection>();
    auto errorConn = std::make_shared<QMetaObject::Connection>();

    *finishConn = connect(e, &ITranslator::finished, this,
        [this, engineId, done, finishConn, errorConn](const QString& t, const QString&) {
            if (*done) return;
            *done = true;
            disconnect(*finishConn);
            disconnect(*errorConn);
            emit testResult(engineId, true, t);
        });
    *errorConn = connect(e, &ITranslator::error, this,
        [this, engineId, done, finishConn, errorConn](const QString& m) {
            if (*done) return;
            *done = true;
            disconnect(*finishConn);
            disconnect(*errorConn);
            emit testResult(engineId, false, m);
        });

    e->translate(text, sourceLang, targetLang);
}

QString TranslationEngineManager::cacheKey(const QString& engineId, const QString& src,
    const QString& tgt, const QString& text) const
{
    const QString raw = engineId + QLatin1Char('\n') + src + QLatin1Char('\n')
        + tgt + QLatin1Char('\n') + text;
    return QString::fromLatin1(QCryptographicHash::hash(raw.toUtf8(), QCryptographicHash::Md5).toHex());
}

bool TranslationEngineManager::cacheLookup(const QString& key, QString& lang, QString& text)
{
    const auto it = m_cache.constFind(key);
    if (it == m_cache.constEnd())
        return false;
    const QString combined = it.value();
    const int sep = combined.indexOf(QLatin1Char('\x1f'));
    if (sep < 0) {
        text = combined;
        lang.clear();
    } else {
        lang = combined.left(sep);
        text = combined.mid(sep + 1);
    }
    return true;
}

void TranslationEngineManager::cachePut(const QString& key, const QString& lang, const QString& text)
{
    const int max = AppConfig::instance().cacheMaxEntries();
    if (max <= 0) {
        m_cache.clear();
        m_cacheOrder.clear();
        return;
    }

    if (!m_cache.contains(key)) {
        m_cacheOrder.append(key);
        while (m_cacheOrder.size() > max) {
            const QString oldest = m_cacheOrder.takeFirst();
            m_cache.remove(oldest);
        }
    }
    m_cache.insert(key, lang + QLatin1Char('\x1f') + text);
}
