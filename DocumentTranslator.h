#ifndef DOCUMENTTRANSLATOR_H
#define DOCUMENTTRANSLATOR_H

#include <QObject>
#include "Document.h"

class TranslationEngineManager;

// 文档翻译编排器：按段落逐段走现有翻译引擎管理器（复用缓存/回退/多引擎），
// 串行处理，保留段落级标题与整体加粗/斜体，支持取消与进度。
class DocumentTranslator : public QObject
{
    Q_OBJECT
public:
    explicit DocumentTranslator(TranslationEngineManager* manager, QObject* parent = nullptr);

    void start(const Document& doc, const QString& sourceLang, const QString& targetLang);
    void cancel();

signals:
    void progress(int done, int total);
    void finished(const Document& doc);
    void error(const QString& message);

private slots:
    void onEngineFinished(const QString& text, const QString& lang);
    void onEngineError(const QString& message);

private:
    void translateNext();
    QString paragraphText(const DocPara& p) const;

    TranslationEngineManager* m_manager;
    QString m_sourceLang;
    QString m_targetLang;

    Document m_src;
    Document m_result;
    int m_cursor = 0;          // 当前处理到的源段落下标
    int m_translatedCount = 0; // 已翻译的非空段落数
    int m_total = 0;           // 需翻译的非空段落总数
    bool m_cancelled = false;
    bool m_awaiting = false;   // 是否正等待引擎返回
};

#endif
