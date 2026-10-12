#include "TranslationEngineManager.h"
#include "ITranslator.h"
#include "AppConfig.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QRandomGenerator>
#include <QStandardPaths>
#include <QTextStream>
#include <QTimer>
#include <memory>

namespace {

// 诊断日志：记录每次翻译请求/结果/错误，便于定位引擎行为（写到应用数据目录 translate_log.txt）。
void logTrans(const QString& msg)
{
    static QString path;
    if (path.isEmpty()) {
        QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        if (dir.isEmpty())
            dir = QStringLiteral(".");
        QDir().mkpath(dir);
        path = dir + QStringLiteral("/translate_log.txt");
    }
    QFile f(path);
    if (f.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream s(&f);
        s << QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz"))
          << " " << msg << "\n";
        f.close();
    }
}

QString preview(const QString& s, int n = 40)
{
    QString t = s;
    t.replace(QLatin1Char('\n'), QLatin1Char(' '));
    t = t.trimmed();
    if (t.size() > n)
        t = t.left(n) + QStringLiteral("…");
    return t;
}

} // namespace

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
        [this, engine](const QString& text, const QString& lang, quint64 requestId) {
            handleFinished(engine, requestId, text, lang);
        });
    connect(engine, &ITranslator::error, this,
        [this, engine](const QString& message, quint64 requestId) {
            handleError(engine, requestId, message);
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
    translateAsync(text, sourceLang, targetLang);
}

quint64 TranslationEngineManager::translateAsync(const QString& text,
    const QString& sourceLang,
    const QString& targetLang)
{
    ITranslator* current = m_engines.value(m_currentId, nullptr);
    const quint64 requestId = m_nextToken++;
    if (!current) {
        emit error(QStringLiteral("未选择翻译引擎"), requestId);
        return requestId;
    }

    const QString key = cacheKey(m_currentId, sourceLang, targetLang, text);
    if (AppConfig::instance().cacheEnabled()) {
        QString lang, cached;
        if (cacheLookup(key, lang, cached)) {
            logTrans(QStringLiteral("[CACHE-HIT] engine=%1 len=%2 → %3")
                         .arg(m_currentId).arg(text.size()).arg(preview(cached)));
            emit finished(cached, lang, requestId);
            return requestId;
        }
    }

    logTrans(QStringLiteral("[REQ] token=%1 engine=%2 len=%3 text=%4")
                 .arg(requestId).arg(m_currentId).arg(text.size()).arg(preview(text)));

    PendingReq req;
    req.text = text;
    req.src = sourceLang;
    req.tgt = targetLang;
    req.cacheKey = key;
    req.engineId = m_currentId;
    m_pending.insert(requestId, req);
    issueRequest(current, requestId);
    return requestId;
}

void TranslationEngineManager::cancelRequest(quint64 requestId)
{
    m_pending.remove(requestId);
    m_activeByToken.remove(requestId);
}

// 限流：若配置了请求最小间隔，则把请求排到下一允许时刻；否则立即发出。
void TranslationEngineManager::issueRequest(ITranslator* engine, quint64 requestId)
{
    if (!engine || !m_pending.contains(requestId))
        return;

    const qint64 interval = AppConfig::instance().docMinIntervalMs();
    const bool throttle = interval > 0
        && engine != m_engines.value(QStringLiteral("offline"), nullptr);
    const qint64 now = QDateTime::currentMSecsSinceEpoch();

    qint64 at = now;
    if (throttle) {
        if (now < m_nextSlotMs)
            at = m_nextSlotMs;
        m_nextSlotMs = qMax(now, m_nextSlotMs) + interval;
    }

    const auto send = [this, engine, requestId]() {
        if (!m_pending.contains(requestId))
            return;
        m_activeByToken.insert(requestId, engine);
        const PendingReq& req = m_pending.value(requestId);
        engine->translate(req.text, req.src, req.tgt, requestId);
    };

    if (at > now)
        QTimer::singleShot(at - now, this, send);
    else
        send();
}

void TranslationEngineManager::handleFinished(ITranslator* e, quint64 requestId,
    const QString& text, const QString& lang)
{
    if (!m_pending.contains(requestId) || m_activeByToken.value(requestId) != e)
        return;

    // 引擎返回空译文（不含错误）：按可重试错误处理，避免静默产出空段落
    if (text.trimmed().isEmpty()) {
        logTrans(QStringLiteral("[EMPTY-RESULT] token=%1 engine=%2").arg(requestId).arg(e->name()));
        handleError(e, requestId, QStringLiteral("引擎返回空译文"));
        return;
    }

    const PendingReq req = m_pending.take(requestId);
    m_activeByToken.remove(requestId);

    logTrans(QStringLiteral("[OK] token=%1 engine=%2 len=%3 → %4")
                 .arg(requestId).arg(e->name()).arg(text.size()).arg(preview(text)));

    if (AppConfig::instance().cacheEnabled())
        cachePut(req.cacheKey, lang, text);

    emit finished(text, lang, requestId);
}

void TranslationEngineManager::handleError(ITranslator* e, quint64 requestId,
    const QString& message)
{
    if (!m_pending.contains(requestId) || m_activeByToken.value(requestId) != e)
        return;

    PendingReq& req = m_pending[requestId];

    // 1) 可重试错误（限流/网络/服务端）：指数退避重试（同引擎同令牌）
    const int maxRetries = AppConfig::instance().docRetryMax();
    if (isRetryableError(message) && req.retryCount < maxRetries) {
        const int base = qMax(1, AppConfig::instance().docRetryBaseMs());
        const int delay = qMin(base * (1 << req.retryCount), 30000)
                        + QRandomGenerator::global()->bounded(base / 2 + 1);
        ++req.retryCount;
        logTrans(QStringLiteral("[RETRY] token=%1 attempt=%2 delay=%3 err=%4")
                     .arg(requestId).arg(req.retryCount).arg(delay).arg(preview(message)));
        QTimer::singleShot(delay, this, [this, requestId]() {
            if (!m_pending.contains(requestId))
                return;
            ITranslator* eng = m_activeByToken.value(requestId, nullptr);
            if (eng)
                issueRequest(eng, requestId);
        });
        return;
    }

    // 2) 在线引擎失败且启用回退：切换到离线引擎重发一次
    ITranslator* offline = m_engines.value(QStringLiteral("offline"), nullptr);
    const bool canFallback = !req.fallbackTried
        && req.engineId != QStringLiteral("offline")
        && offline
        && AppConfig::instance().fallbackEnabled()
        && AppConfig::instance().offlineEnabled();
    if (canFallback) {
        req.fallbackTried = true;
        logTrans(QStringLiteral("[FALLBACK] token=%1 → offline").arg(requestId));
        issueRequest(offline, requestId);
        return;
    }

    // 3) 最终失败
    m_pending.remove(requestId);
    m_activeByToken.remove(requestId);
    logTrans(QStringLiteral("[ERROR] token=%1 engine=%2 err=%3")
                 .arg(requestId).arg(e->name()).arg(preview(message, 120)));
    emit error(message, requestId);
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
    const quint64 requestId = m_nextToken++;

    *finishConn = connect(this, &TranslationEngineManager::finished, this,
        [this, engineId, done, finishConn, errorConn, requestId](
            const QString& t, const QString&, quint64 rid) {
            if (rid != requestId || *done) return;
            *done = true;
            disconnect(*finishConn);
            disconnect(*errorConn);
            emit testResult(engineId, true, t);
        });
    *errorConn = connect(this, &TranslationEngineManager::error, this,
        [this, engineId, done, finishConn, errorConn, requestId](
            const QString& m, quint64 rid) {
            if (rid != requestId || *done) return;
            *done = true;
            disconnect(*finishConn);
            disconnect(*errorConn);
            emit testResult(engineId, false, m);
        });

    PendingReq req;
    req.text = text;
    req.src = sourceLang;
    req.tgt = targetLang;
    req.cacheKey = cacheKey(engineId, sourceLang, targetLang, text);
    req.engineId = engineId;
    m_pending.insert(requestId, req);
    issueRequest(e, requestId);
}

// 可重试错误白名单（不区分大小写）。限流/网络/服务端类错误值得退避重试；
// 签名错误（54001）、参数错误（54000/54004 等）与用户取消不在此列。
bool TranslationEngineManager::isRetryableError(const QString& message)
{
    static const QStringList patterns = {
        QStringLiteral("52001"), QStringLiteral("54003"), QStringLiteral("54005"),
        QStringLiteral("429"), QStringLiteral("502"), QStringLiteral("503"),
        QStringLiteral("timeout"), QStringLiteral("timed out"), QStringLiteral("refused"),
        QStringLiteral("host not found"), QStringLiteral("temporary network"),
        QStringLiteral("rate limit"), QStringLiteral("too many"),
        QStringLiteral("connection"), QStringLiteral("network"), QStringLiteral("server"),
        // libcurl / TLS 瞬时失败：SSL 握手、连接、DNS、收发中断等，重试通常可恢复
        QStringLiteral("ssl"), QStringLiteral("tls"), QStringLiteral("handshake"),
        QStringLiteral("connect"), QStringLiteral("resolve"), QStringLiteral("recv"),
        QStringLiteral("send fail"), QStringLiteral("reset"), QStringLiteral("eof"),
        QStringLiteral("closed"), QStringLiteral("peer"), QStringLiteral("certificate"),
    };
    const QString low = message.toLower();
    for (const QString& p : patterns) {
        if (low.contains(p))
            return true;
    }
    return false;
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
