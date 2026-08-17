#ifndef TRANSLATIONSERVICE_H
#define TRANSLATIONSERVICE_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>

class TranslationService : public QObject
{
    Q_OBJECT
public:
    explicit TranslationService(QObject* parent = nullptr);

    // 发起翻译请求，异步返回结果
    void translate(const QString& text,
        const QString& sourceLang = "auto",   // 自动检测
        const QString& targetLang = "zh");

signals:
    void translationFinished(const QString& translatedText, const QString& detectedLang);
    void translationError(const QString& errorMessage);

private slots:
    void onReplyFinished(QNetworkReply* reply);

private:
    QNetworkAccessManager* networkManager;
};

#endif