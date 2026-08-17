#ifndef ITRANSLATOR_H
#define ITRANSLATOR_H

#include <QObject>
#include <QString>

// 翻译引擎统一接口。内置 Google 引擎与外部插件翻译引擎都实现此接口，
// 以便在设置页运行时切换。
class ITranslator : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;

    virtual QString name() const = 0;
    virtual void translate(const QString& text, const QString& sourceLang, const QString& targetLang) = 0;

signals:
    void finished(const QString& translatedText, const QString& detectedLang);
    void error(const QString& message);
};

#endif
