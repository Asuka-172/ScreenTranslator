#ifndef DOCXEXPORTER_H
#define DOCXEXPORTER_H

#include <QString>
#include <QStringList>

// 生成最小可用的 .docx（OOXML）文件，零外部依赖：
// 手写 zip 打包（STORE 不压缩）+ [Content_Types].xml / _rels/.rels / word/document.xml
class DocxExporter
{
public:
    // 每个 entry 作为一个段落（段落内换行转为软换行），entry 之间空段分隔
    static bool write(const QString& filePath, const QStringList& entries);
};

#endif
