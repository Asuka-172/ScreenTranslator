#ifndef CUSTOMONLINETRANSLATOR_H
#define CUSTOMONLINETRANSLATOR_H

#include "ITranslator.h"

#include <QHash>

class QNetworkAccessManager;
class QNetworkReply;

// 通用自定义在线翻译引擎：用户提供 URL 模板（含 {text} {from} {to} {key} 占位符）
// 与可选的 API Key，响应按 JSON 结果路径提取译文。
class CustomOnlineTranslator : public ITranslator
{
    Q_OBJECT
public:
    explicit CustomOnlineTranslator(QObject* parent = nullptr);

    QString name() const override;
    void translate(const QString& text, const QString& sourceLang, const QString& targetLang,
                   quint64 requestId) override;

private slots:
    void onReplyFinished(QNetworkReply* reply);

private:
    static QString extractByPath(const QByteArray& data, const QString& path);

    QNetworkAccessManager* m_nm;
    // 在途请求：reply -> 请求令牌（并发时区分结果归属）
    QHash<QNetworkReply*, quint64> m_pending;
};

#endif
