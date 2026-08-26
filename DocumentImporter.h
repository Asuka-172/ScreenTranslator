#ifndef DOCUMENTIMPORTER_H
#define DOCUMENTIMPORTER_H

#include "Document.h"

// 按扩展名分发，从 .txt / .docx / .pdf 提取文本与基本格式，返回结构化文档。
class DocumentImporter
{
public:
    static Document import(const QString& path, QString* err = nullptr);
};

#endif
