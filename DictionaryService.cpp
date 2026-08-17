#include "DictionaryService.h"

#include <QUrl>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSslSocket>
#include <QSslConfiguration>
#include <QNetworkRequest>

namespace {

bool parseDictionary(const QByteArray& data, const QString& word, QString& formatted)
{
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isArray())
        return false;

    QJsonArray root = doc.array();
    if (root.isEmpty())
        return false;

    QJsonObject entry = root[0].toObject();

    QString phonetic = entry["phonetic"].toString();
    if (phonetic.isEmpty()) {
        const QJsonArray phonetics = entry["phonetics"].toArray();
        if (!phonetics.isEmpty())
            phonetic = phonetics[0].toObject()["text"].toString();
    }

    QStringList lines;
    QString title = word;
    if (!phonetic.isEmpty())
        title += "  [" + phonetic + "]";
    lines << title;

    const QJsonArray meanings = entry["meanings"].toArray();
    for (const QJsonValue& mv : meanings) {
        const QJsonObject meaning = mv.toObject();
        const QString pos = meaning["partOfSpeech"].toString();
        if (!pos.isEmpty())
            lines << QString() << pos;
        const QJsonArray defs = meaning["definitions"].toArray();
        for (const QJsonValue& dv : defs) {
            const QJsonObject def = dv.toObject();
            const QString d = def["definition"].toString();
            if (!d.isEmpty())
                lines << "  - " + d;
            const QString example = def["example"].toString();
            if (!example.isEmpty())
                lines << "    例: " + example;
        }
    }

    if (lines.size() <= 1)
        return false;

    formatted = lines.join('\n');
    return true;
}

} // namespace

DictionaryService::DictionaryService(QObject* parent)
    : QObject(parent)
    , m_manager(new QNetworkAccessManager(this))
{
}

void DictionaryService::lookup(const QString& word)
{
    const QString w = word.trimmed().toLower();
    if (w.isEmpty())
        return;

    QUrl url(QString("https://api.dictionaryapi.dev/api/v2/entries/en/%1").arg(w));
    QNetworkRequest request(url);

    QSslConfiguration ssl = request.sslConfiguration();
    ssl.setPeerVerifyMode(QSslSocket::VerifyNone);
    request.setSslConfiguration(ssl);

    QNetworkReply* reply = m_manager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, w]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            emit error(reply->errorString());
            return;
        }

        QString formatted;
        if (parseDictionary(reply->readAll(), w, formatted))
            emit resultReady(w, formatted);
        else
            emit error("未查询到释义");
    });
}
