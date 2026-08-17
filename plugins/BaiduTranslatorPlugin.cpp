#include "BaiduTranslatorPlugin.h"

#include <QUrl>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCryptographicHash>
#include <QDateTime>
#include <curl/curl.h>

namespace {

QString baiduLang(const QString& lang)
{
    if (lang == "zh-CN" || lang == "zh-TW" || lang == "zh") return "zh";
    if (lang == "ja")  return "jp";
    if (lang == "ko")  return "kor";
    if (lang == "fr")  return "fra";
    if (lang == "es")  return "spa";
    if (lang == "auto") return "auto";
    return lang;  // en / de 等与百度一致
}

size_t writeCallback(void* contents, size_t size, size_t nmemb, void* userp)
{
    const size_t total = size * nmemb;
    static_cast<QByteArray*>(userp)->append(static_cast<char*>(contents), static_cast<int>(total));
    return total;
}

QString md5(const QString& s)
{
    return QString::fromLatin1(QCryptographicHash::hash(s.toUtf8(), QCryptographicHash::Md5).toHex());
}

} // namespace

QString BaiduTranslatorPlugin::name() const { return QStringLiteral("百度翻译"); }
QString BaiduTranslatorPlugin::version() const { return QStringLiteral("1.0.0"); }

bool BaiduTranslatorPlugin::init() { return true; }
void BaiduTranslatorPlugin::shutdown() {}

QString BaiduTranslatorPlugin::translate(const QString& text, const QString& sourceLang,
                                         const QString& targetLang, QString& error)
{
    const QString appid = qEnvironmentVariable("BAIDU_APPID");
    const QString secret = qEnvironmentVariable("BAIDU_SECRET");
    if (appid.isEmpty() || secret.isEmpty()) {
        error = QStringLiteral("未配置 BAIDU_APPID / BAIDU_SECRET");
        return QString();
    }

    const QString salt = QString::number(QDateTime::currentMSecsSinceEpoch());
    const QString sign = md5(appid + text + salt + secret);

    QUrl url(QStringLiteral("https://fanyi-api.baidu.com/api/trans/vip/translate"));
    QUrlQuery query;
    query.addQueryItem("q", text);
    query.addQueryItem("from", baiduLang(sourceLang));
    query.addQueryItem("to", baiduLang(targetLang));
    query.addQueryItem("appid", appid);
    query.addQueryItem("salt", salt);
    query.addQueryItem("sign", sign);
    url.setQuery(query);

    CURL* curl = curl_easy_init();
    if (!curl) {
        error = QStringLiteral("libcurl 初始化失败");
        return QString();
    }

    QByteArray response;
    curl_easy_setopt(curl, CURLOPT_URL, url.toString().toUtf8().constData());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    const CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        error = QString("请求失败: %1").arg(QString::fromLatin1(curl_easy_strerror(res)));
        return QString();
    }

    const QJsonDocument doc = QJsonDocument::fromJson(response);
    if (!doc.isObject()) {
        error = QStringLiteral("响应解析失败");
        return QString();
    }

    const QJsonObject obj = doc.object();
    if (obj.contains("error_code")) {
        error = QString("百度翻译错误 %1: %2")
                    .arg(obj["error_code"].toString(), obj["error_msg"].toString());
        return QString();
    }

    QString result;
    const QJsonArray arr = obj["trans_result"].toArray();
    for (const QJsonValue& v : arr)
        result += v.toObject()["dst"].toString();

    if (result.isEmpty()) {
        error = QStringLiteral("翻译结果为空");
        return QString();
    }
    return result;
}

extern "C" IPlugin* createPlugin()
{
    return new BaiduTranslatorPlugin();
}
