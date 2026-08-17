#include "TranslationService.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrlQuery>
#include <QSslSocket>
#include <QSslConfiguration>

TranslationService::TranslationService(QObject* parent)
    : ITranslator(parent),
    networkManager(new QNetworkAccessManager(this))
{
}

QString TranslationService::name() const
{
    return QStringLiteral("Google");
}

void TranslationService::translate(const QString& text,
    const QString& sourceLang,
    const QString& targetLang)
{
    QUrl url("https://translate.googleapis.com/translate_a/single");
    QUrlQuery query;
    query.addQueryItem("client", "gtx");
    query.addQueryItem("sl", sourceLang == "auto" ? "auto" : sourceLang);
    query.addQueryItem("tl", targetLang);
    query.addQueryItem("dt", "t");
    query.addQueryItem("q", text);
    url.setQuery(query);

    qDebug() << "Translation URL:" << url.toString();

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    QSslConfiguration sslConfig = request.sslConfiguration();
    sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);
    request.setSslConfiguration(sslConfig);

    QNetworkReply* reply = networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onReplyFinished(reply);
        });
}

void TranslationService::onReplyFinished(QNetworkReply* reply)
{
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        emit error(reply->errorString());
        return;
    }

    QByteArray responseData = reply->readAll();
    // Google 返回的是一个 JSON 数组，格式为：
    // [[["翻译文本", "原文", null, null, 1]], null, "en"]
    // 取第一个部分的第一个数组的第一个元素。
    QJsonDocument doc = QJsonDocument::fromJson(responseData);
    if (!doc.isArray()) {
        emit error("Invalid response format");
        return;
    }

    QJsonArray rootArray = doc.array();
    if (rootArray.isEmpty() || !rootArray[0].isArray()) {
        emit error("Empty translation result");
        return;
    }

    QJsonArray firstSegment = rootArray[0].toArray();
    if (firstSegment.isEmpty() || !firstSegment[0].isArray()) {
        emit error("No translation segments");
        return;
    }

    QString translatedText;
    // 翻译结果可能分为多个片段，依次拼接：
    for (const QJsonValue& val : firstSegment) {
        QJsonArray part = val.toArray();
        if (!part.isEmpty()) {
            translatedText += part[0].toString();
        }
    }

    // 检测语言：取返回数组的第3个元素（第2个为源语言代码）
    QString detectedLang = "??";
    if (rootArray.size() > 2 && rootArray[2].isString()) {
        detectedLang = rootArray[2].toString();
    }

    if (translatedText.isEmpty()) {
        emit error("Translation empty");
        return;
    }

    emit finished(translatedText, detectedLang);
}