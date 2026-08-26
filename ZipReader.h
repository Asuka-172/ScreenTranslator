#ifndef ZIPREADER_H
#define ZIPREADER_H

#include <QString>
#include <QByteArray>
#include <QList>

// 最小 zip 读取器：仅解析 local header + central directory，
// 支持 STORE(0) 与 DEFLATE(8) 两种方法，用于读取 .docx（OOXML 容器）。
class ZipReader
{
public:
    bool load(const QString& zipPath, QString* err = nullptr);
    bool hasEntry(const QString& entryName) const;
    QByteArray readEntry(const QString& entryName, bool* ok = nullptr) const;

private:
    struct Entry {
        QString name;
        quint32 localOffset;
        quint32 compSize;
        quint32 uncompSize;
        quint16 method;
    };

    QByteArray m_data;
    QList<Entry> m_entries;
};

#endif
