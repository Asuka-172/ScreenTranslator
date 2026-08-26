#ifndef DOCUMENT_H
#define DOCUMENT_H

#include <QString>
#include <QList>

// 文档中间表示：提取后的结构化文档，供导入/翻译/导出共用。
// 保真度目标：段落结构 + 标题层级 + 加粗/斜体（段落级）。

struct DocRun
{
    QString text;
    bool bold = false;
    bool italic = false;
    bool underline = false;
};

struct DocPara
{
    QList<DocRun> runs;
    int headingLevel = 0; // 0=正文, 1-6=标题层级
};

struct Document
{
    QString title;
    QList<DocPara> paragraphs;
};

#endif
