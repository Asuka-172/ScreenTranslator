#ifndef DOCOUTPUTSESSION_H
#define DOCOUTPUTSESSION_H

#include "Document.h"

#include <QString>

class QFile;
class QPdfWriter;

// 增量译文输出会话：把按原文顺序提交的翻译段落「接续」写入同一个输出文件，
// 保证文件中始终包含已提交前缀 —— 中途失败/取消/崩溃后，该文件即半成品。
//
// - txt：整体重写（内存累积 + 周期落盘，逻辑简单且与续传一致）
// - docx：内存累积 + 周期整体重写（zip 为 STORE 无压缩，重写开销小）
// - pdf：保持 QPdfWriter 打开，每批段落打印追加页
//
// 续传场景：open() 时以已提交段落作为种子，后续 append 只追加新段落。
class DocOutputSession
{
public:
    enum class Format { Txt, Docx, Pdf };

    explicit DocOutputSession(const QString& filePath, Format fmt);
    ~DocOutputSession();

    static Format formatFromSuffix(const QString& filePath);

    // 打开输出：seed 为已提交（续传）段落，空文档表示全新开始。
    bool open(const Document& seed, QString* err = nullptr);
    // 追加一段译文（必须严格按文档顺序调用）。
    bool append(const DocPara& para, QString* err = nullptr);
    // 强制落盘（txt/docx 重写；pdf 无操作）。
    bool flush(QString* err = nullptr);
    // 结束：最终落盘并关闭（pdf 结束写入）。
    bool finish(QString* err = nullptr);
    // 放弃：尽力关闭，保留已落盘内容。
    void abort();

private:
    void writeTxtAll();
    void printParaToPdf(const DocPara& para);
    void printParasToPdf(const QList<DocPara>& paras);

    QString m_filePath;
    Format m_fmt;
    QFile* m_txtFile = nullptr;
    QPdfWriter* m_pdfWriter = nullptr;
    Document m_acc;   // 累积的已提交译文（内部格式）
    bool m_closed = false;
};

#endif
