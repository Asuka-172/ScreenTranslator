#ifndef TRANSLATIONSERVICE_H
#define TRANSLATIONSERVICE_H

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QHash>
#include "ITranslator.h"

// 内置 Google 翻译引擎，实现 ITranslator 接口。
class TranslationService : public ITranslator
{
    Q_OBJECT
public:
    explicit TranslationService(QObject* parent = nullptr);

    QString name() const override;
    void translate(const QString& text, const QString& sourceLang, const QString& targetLang,
                   quint64 requestId) override;

private slots:
    void onReplyFinished(QNetworkReply* reply);

private:
    QNetworkAccessManager* networkManager;
    // 在途请求：reply -> 请求令牌（并发时区分结果归属）
    QHash<QNetworkReply*, quint64> m_pending;
};

#endif
