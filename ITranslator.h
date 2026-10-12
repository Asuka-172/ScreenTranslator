#ifndef ITRANSLATOR_H
#define ITRANSLATOR_H

#include <QObject>
#include <QString>

// 翻译引擎统一接口。内置 Google 引擎与外部插件翻译引擎都实现此接口，
// 以便在设置页运行时切换。
//
// requestId：调用方（TranslationEngineManager）分配的唯一请求令牌。
// 引擎必须把该令牌原样随 finished/error 信号带回，供管理器在并发请求
// （文档翻译多路流水线）下按令牌路由结果。
class ITranslator : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;

    virtual QString name() const = 0;
    virtual void translate(const QString& text, const QString& sourceLang,
                           const QString& targetLang, quint64 requestId) = 0;

signals:
    void finished(const QString& translatedText, const QString& detectedLang, quint64 requestId);
    void error(const QString& message, quint64 requestId);
};

#endif
