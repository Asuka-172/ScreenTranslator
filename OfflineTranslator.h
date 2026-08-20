#ifndef OFFLINETRANSLATOR_H
#define OFFLINETRANSLATOR_H

#include "ITranslator.h"

#include <QList>
#include <QPair>
#include <QDateTime>

// 离线翻译引擎（本地词典降级占位）。真正可离线运行：读取词典文件逐词/短语替换。
// 预留 models 目录检测与模型路径配置，后续可替换内部实现为 Bergamot/Marian。
class OfflineTranslator : public ITranslator
{
    Q_OBJECT
public:
    explicit OfflineTranslator(QObject* parent = nullptr);

    QString name() const override;
    void translate(const QString& text, const QString& sourceLang, const QString& targetLang) override;

    void reloadGlossary();
    QStringList listModelFiles() const;

    static QString defaultModelDir();
    static QString defaultGlossaryPath();

private:
    QString resolvedModelDir() const;
    QString resolvedGlossaryPath() const;
    void loadGlossary();

    QList<QPair<QString, QString>> m_entries;
    QDateTime m_glossaryMtime;
};

#endif
