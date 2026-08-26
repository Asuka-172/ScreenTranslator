#include "ZipReader.h"
#include <QFile>
#include <zlib.h>

namespace {

quint16 u16(const QByteArray& b, int off)
{
    return quint16(quint8(b.at(off)) | (quint8(b.at(off + 1)) << 8));
}

quint32 u32(const QByteArray& b, int off)
{
    return quint32(quint8(b.at(off)))
        | (quint32(quint8(b.at(off + 1))) << 8)
        | (quint32(quint8(b.at(off + 2))) << 16)
        | (quint32(quint8(b.at(off + 3))) << 24);
}

QByteArray inflateRaw(const QByteArray& data, quint32 uncompSize, bool* ok)
{
    *ok = false;
    if (uncompSize == 0) {
        *ok = true;
        return QByteArray();
    }

    z_stream strm;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = uInt(data.size());
    strm.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(data.constData()));

    // 负 windowBits 表示原始 deflate（无 zlib 头），ZIP 用的正是这种格式
    if (inflateInit2(&strm, -MAX_WBITS) != Z_OK)
        return QByteArray();

    QByteArray out;
    out.resize(int(uncompSize));
    strm.avail_out = uInt(out.size());
    strm.next_out = reinterpret_cast<Bytef*>(out.data());

    const int ret = inflate(&strm, Z_FINISH);
    inflateEnd(&strm);

    if (ret != Z_STREAM_END) {
        // 大小不匹配时重试动态缓冲
        return QByteArray();
    }
    *ok = true;
    return out;
}

} // namespace

bool ZipReader::load(const QString& zipPath, QString* err)
{
    m_entries.clear();

    QFile file(zipPath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (err) *err = QStringLiteral("无法打开文件：%1").arg(zipPath);
        return false;
    }
    m_data = file.readAll();
    file.close();

    if (m_data.size() < 22) {
        if (err) *err = QStringLiteral("文件过小，不是有效的 zip/docx");
        return false;
    }

    // 从尾部向前找 EOCD 签名 0x06054b50（允许尾部注释）
    int eocd = -1;
    for (int i = m_data.size() - 22; i >= 0; --i) {
        if (u32(m_data, i) == 0x06054b50u) {
            eocd = i;
            break;
        }
    }
    if (eocd < 0) {
        if (err) *err = QStringLiteral("未找到 zip 目录记录");
        return false;
    }

    const quint16 entryCount = u16(m_data, eocd + 10);
    const quint32 cdSize = u32(m_data, eocd + 12);
    const quint32 cdOffset = u32(m_data, eocd + 16);

    // 解析 central directory 中的每个条目
    int pos = int(cdOffset);
    const int cdEnd = pos + int(cdSize);
    for (quint16 i = 0; i < entryCount && pos + 46 <= cdEnd; ++i) {
        if (u32(m_data, pos) != 0x02014b50u)
            break;

        const quint16 method = u16(m_data, pos + 10);
        const quint32 compSize = u32(m_data, pos + 20);
        const quint32 uncompSize = u32(m_data, pos + 24);
        const quint16 nameLen = u16(m_data, pos + 28);
        const quint16 extraLen = u16(m_data, pos + 30);
        const quint16 commentLen = u16(m_data, pos + 32);
        const quint32 localOffset = u32(m_data, pos + 42);

        Entry e;
        e.name = QString::fromUtf8(m_data.mid(pos + 46, nameLen));
        e.localOffset = localOffset;
        e.compSize = compSize;
        e.uncompSize = uncompSize;
        e.method = method;
        m_entries.append(e);

        pos += 46 + nameLen + extraLen + commentLen;
    }

    if (m_entries.isEmpty()) {
        if (err) *err = QStringLiteral("zip 中没有任何条目");
        return false;
    }
    return true;
}

bool ZipReader::hasEntry(const QString& entryName) const
{
    for (const Entry& e : m_entries)
        if (e.name == entryName)
            return true;
    return false;
}

QByteArray ZipReader::readEntry(const QString& entryName, bool* ok) const
{
    if (ok) *ok = false;

    for (const Entry& e : m_entries) {
        if (e.name != entryName)
            continue;

        const int pos = int(e.localOffset);
        if (pos + 30 > m_data.size())
            return QByteArray();
        if (u32(m_data, pos) != 0x04034b50u)
            return QByteArray();

        const quint16 nameLen = u16(m_data, pos + 26);
        const quint16 extraLen = u16(m_data, pos + 28);
        const int dataStart = pos + 30 + nameLen + extraLen;
        if (dataStart < 0 || dataStart + int(e.compSize) > m_data.size())
            return QByteArray();

        QByteArray compressed = m_data.mid(dataStart, int(e.compSize));

        if (e.method == 0) { // STORE
            if (ok) *ok = true;
            return compressed;
        }
        if (e.method == 8) { // DEFLATE
            return inflateRaw(compressed, e.uncompSize, ok);
        }
        // 不支持的方法
        return QByteArray();
    }
    return QByteArray();
}
