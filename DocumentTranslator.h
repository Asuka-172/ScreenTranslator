#ifndef DOCUMENTTRANSLATOR_H
#define DOCUMENTTRANSLATOR_H

#include <QObject>
#include <QHash>
#include <QList>
#include <QPair>
#include "Document.h"

class TranslationEngineManager;
class DocOutputSession;

// 文档翻译编排器。
//
// 工作模型（长文分段 + 分批提交 + 接续写入同一译文文件）：
//   1. 整篇预分段：每个非空段落为翻译单元；段落内按「相邻同样式 run」归组，
//      组文本超过 docChunkMaxBytes（默认 6000 字节）时按句/词边界切块。
//   2. 有界并发：向 TranslationEngineManager 以令牌方式提交，最多
//      docConcurrency 个请求在途。
//   3. 按序提交：结果按原文顺序（段落序）提交 —— 只有当某段落的所有块都完成
//      时才组装该段并追加到译文，保证输出严格对应原文结构。
//   4. 接续写入：通过 DocOutputSession 把每段译文持续写入同一个输出文件
//      （txt/docx/pdf），文件中始终包含已提交前缀，即半成品。
//   5. 断点续传：每提交一段落盘 checkpoint（内部格式存储源文档与已译段落），
//      失败/取消/崩溃后可续传。
class DocumentTranslator : public QObject
{
    Q_OBJECT
public:
    // 翻译单元（段）：某段落内的一个样式组的一个分块
    struct Segment {
        int paraIndex = 0;     // 源段落下标
        int groupIndex = 0;    // 样式组序号
        int chunkIndex = 0;    // 组内块序号
        int groupChunks = 1;   // 该组总块数
        QString text;
        bool bold = false;
        bool italic = false;
        bool underline = false;
        bool formula = false;  // 公式：不送引擎，原样保留
    };

    // 断点数据：全部按内部格式（Document）存取，保证与导出格式一致
    struct ResumeData {
        Document source;              // 源文档（续传时不重新导入）
        QList<DocPara> translated;    // 已提交译文（按原文顺序）
        QString srcLang, tgtLang, engineId, inputPath, outputPath;
        int nextPara = 0;             // 下一个待翻译段落下标
        int doneNonEmpty = 0;
        int totalNonEmpty = 0;
        int chunkMaxBytes = 0;
    };

    explicit DocumentTranslator(TranslationEngineManager* manager, QObject* parent = nullptr);
    ~DocumentTranslator() override;

    // 开始翻译。resumeNextPara/seedTranslated 供断点续传使用（新译传 0/空）。
    void start(const Document& doc, const QString& sourceLang, const QString& targetLang,
               const QString& inputPath, const QString& outputPath,
               int resumeNextPara = 0, const QList<DocPara>& seedTranslated = QList<DocPara>());
    void cancel();
    int doneCount() const;   // 已提交的非空段落数

    // 断点续传辅助
    static QString resumeDir();
    static QString resumePathFor(const QString& inputPath);
    static bool hasResume(const QString& inputPath);
    static bool loadCheckpoint(const QString& inputPath, ResumeData& out);
    static void removeCheckpoint(const QString& inputPath);

signals:
    void progress(int done, int total, const QString& detail);
    void finished(const Document& doc);
    void error(const QString& message);

private slots:
    void onEngineFinished(const QString& text, const QString& lang, quint64 requestId);
    void onEngineError(const QString& message, quint64 requestId);

private:
    // 分段
    static QStringList chunkText(const QString& text, int maxBytes);
    void buildSegments();
    QString paragraphText(const DocPara& p) const;
    bool isParaEmpty(const DocPara& p) const;

    // 调度
    void feedWorkers();
    void tryCommit();
    void assemblePara(int paraIndex, DocPara& out) const;
    void saveCheckpoint();
    void failAndStop(const QString& message);

    TranslationEngineManager* m_manager;
    QString m_sourceLang, m_targetLang, m_inputPath, m_outputPath;

    DocOutputSession* m_output = nullptr;
    Document m_src;
    Document m_result;            // 已提交译文（内部格式，按原文顺序）
    QList<Segment> m_segments;    // 整篇分段（预生成，段落序）
    QList<QPair<int, int>> m_paraSegRange;   // 段落 -> [首段下标, 末段下标]（空段为 [-1,-1]）
    QHash<int, int> m_paraPending;           // 段落 -> 尚未完成的分段数（含未投递；为 0 表示可提交）
    QHash<int, QString> m_segResults;        // 段下标 -> 译文
    QHash<quint64, int> m_tokenToSeg;        // 请求令牌 -> 段下标

    int m_nextFeed = 0;      // 下一个待投递段下标
    int m_nextPara = 0;      // 下一个待提交段落下标
    int m_inFlight = 0;
    int m_maxInFlight = 3;
    int m_totalNonEmpty = 0; // 非空段落总数（进度分母）
    int m_doneNonEmpty = 0;  // 已提交非空段落数（进度分子）
    bool m_cancelled = false;
    bool m_failed = false;
};

#endif
