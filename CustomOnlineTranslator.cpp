#include "CustomOnlineTranslator.h"
#include "AppConfig.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QSslSocket>
#include <QSslConfiguration>

CustomOnlineTranslator::CustomOnlineTranslator(QObject* parent)
    : ITranslator(parent)
    , m_nm(new QNetworkAccessManager(this))
{
}

QString CustomOnlineTranslator::name() const
{
    return QStringLiteral("自定义在线引擎");
}

void CustomOnlineTranslator::translate(const QString& text,
    const QString& sourceLang,
    const QString& targetLang)
{
    AppConfig& cfg = AppConfig::instance();
    const QString urlTemplate = cfg.customEngineUrl().trimmed();
    if (urlTemplate.isEmpty()) {
        emit error(QStringLiteral("未配置自定义引擎 URL，请在设置中填写"));
        return;
    }

    const QString apiKey = cfg.customEngineApiKey();
    const QString keyHeader = cfg.customEngineKeyHeader().trimmed();

    // 替换占位符。{text} 需 URL 编码，其余直接替换。
    QString url = urlTemplate;
    url.replace(QStringLiteral("{text}"), QUrl::toPercentEncoding(text));
    url.replace(QStringLiteral("{from}"), sourceLang);
    url.replace(QStringLiteral("{to}"), targetLang);
    if (keyHeader.isEmpty())
        url.replace(QStringLiteral("{key}"), QUrl::toPercentEncoding(apiKey));
    else
        url.replace(QStringLiteral("{key}"), QString());

    QNetworkRequest request{QUrl(url)};
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    if (!keyHeader.isEmpty() && !apiKey.isEmpty())
        request.setRawHeader(keyHeader.toUtf8(), apiKey.toUtf8());

    QSslConfiguration sslConfig = request.sslConfiguration();
    sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);
    request.setSslConfiguration(sslConfig);

    QNetworkReply* reply = m_nm->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onReplyFinished(reply);
    });
}

void CustomOnlineTranslator::onReplyFinished(QNetworkReply* reply)
{
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        emit error(reply->errorString());
        return;
    }

    const QByteArray data = reply->readAll();
    const QString path = AppConfig::instance().customEngineResultPath().trimmed();
    const QString result = extractByPath(data, path);

    if (result.isEmpty()) {
        emit error(QStringLiteral("翻译结果为空（请检查结果路径配置）"));
        return;
    }

    emit finished(result, QString());
}

// 从 JSON 响应中按点分路径（对象键 + 数字数组下标）提取译文。
// 路径为空时，把整个响应体当作译文（纯文本接口）。
QString CustomOnlineTranslator::extractByPath(const QByteArray& data, const QString& path)
{
    if (path.isEmpty())
        return QString::fromUtf8(data).trimmed();

    const QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull())
        return QString();

    QJsonValue current = doc.isArray() ? QJsonValue(doc.array()) : QJsonValue(doc.object());

    const QStringList segments = path.split('.', Qt::SkipEmptyParts);
    for (const QString& seg : segments) {
        if (current.isObject()) {
            current = current.toObject().value(seg);
        } else if (current.isArray()) {
            bool ok = false;
            const int idx = seg.toInt(&ok);
            if (!ok)
                return QString();
            current = current.toArray().at(idx);
        } else {
            return QString();
        }
        if (current.isUndefined())
            return QString();
    }

    if (current.isString())
        return current.toString();
    if (current.isArray()) {
        QString joined;
        const QJsonArray arr = current.toArray();
        for (const QJsonValue& v : arr) {
            if (v.isString())
                joined += v.toString();
            else if (v.isObject())
                joined += QString::fromUtf8(QJsonDocument(v.toObject()).toJson(QJsonDocument::Compact));
            else if (v.isArray())
                joined += QString::fromUtf8(QJsonDocument(v.toArray()).toJson(QJsonDocument::Compact));
        }
        return joined;
    }
    if (current.isDouble())
        return QString::number(current.toDouble());
    return QString();
}
