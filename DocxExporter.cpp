#include "DocxExporter.h"
#include <QFile>
#include <QByteArray>
#include <QList>

namespace {

void appendU16(QByteArray& out, quint16 v)
{
    out.append(char(v & 0xFF));
    out.append(char((v >> 8) & 0xFF));
}

void appendU32(QByteArray& out, quint32 v)
{
    out.append(char(v & 0xFF));
    out.append(char((v >> 8) & 0xFF));
    out.append(char((v >> 16) & 0xFF));
    out.append(char((v >> 24) & 0xFF));
}

quint32 crc32(const QByteArray& data)
{
    static quint32 table[256];
    static bool initialized = false;
    if (!initialized) {
        for (quint32 i = 0; i < 256; ++i) {
            quint32 c = i;
            for (int k = 0; k < 8; ++k)
                c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            table[i] = c;
        }
        initialized = true;
    }

    quint32 crc = 0xFFFFFFFFu;
    for (unsigned char b : data)
        crc = table[(crc ^ b) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}

struct ZipEntry {
    QString name;
    QByteArray data;
};

// 构建 zip 归档（所有条目 STORE 不压缩）
QByteArray buildZip(const QList<ZipEntry>& entries)
{
    QByteArray out;
    QByteArray centralDir;
    quint32 offset = 0;

    for (const ZipEntry& e : entries) {
        const QByteArray nameBytes = e.name.toUtf8();
        const quint32 crc = crc32(e.data);
        const quint32 size = quint32(e.data.size());

        QByteArray local;
        appendU32(local, 0x04034b50);      // local file header signature
        appendU16(local, 20);              // version needed
        appendU16(local, 0);               // flags
        appendU16(local, 0);               // method: STORE
        appendU16(local, 0);               // mod time
        appendU16(local, 0x5021);          // mod date (2020-01-01)
        appendU32(local, crc);
        appendU32(local, size);            // compressed size
        appendU32(local, size);            // uncompressed size
        appendU16(local, quint16(nameBytes.size()));
        appendU16(local, 0);               // extra length
        local.append(nameBytes);

        out.append(local);
        out.append(e.data);

        QByteArray cd;
        appendU32(cd, 0x02014b50);         // central dir header signature
        appendU16(cd, 20);                 // version made by
        appendU16(cd, 20);                 // version needed
        appendU16(cd, 0);                  // flags
        appendU16(cd, 0);                  // method: STORE
        appendU16(cd, 0);                  // mod time
        appendU16(cd, 0x5021);             // mod date
        appendU32(cd, crc);
        appendU32(cd, size);               // compressed size
        appendU32(cd, size);               // uncompressed size
        appendU16(cd, quint16(nameBytes.size()));
        appendU16(cd, 0);                  // extra length
        appendU16(cd, 0);                  // comment length
        appendU16(cd, 0);                  // disk number start
        appendU16(cd, 0);                  // internal attrs
        appendU32(cd, 0);                  // external attrs
        appendU32(cd, offset);             // local header offset
        cd.append(nameBytes);

        centralDir.append(cd);
        offset = quint32(out.size());
    }

    const quint32 cdOffset = quint32(out.size());
    out.append(centralDir);
    const quint32 cdSize = quint32(centralDir.size());

    appendU32(out, 0x06054b50);            // EOCD signature
    appendU16(out, 0);
    appendU16(out, 0);
    appendU16(out, quint16(entries.size()));
    appendU16(out, quint16(entries.size()));
    appendU32(out, cdSize);
    appendU32(out, cdOffset);
    appendU16(out, 0);                     // comment length

    return out;
}

QString escapeXml(const QString& s)
{
    QString out = s;
    out.replace("&", "&amp;");
    out.replace("<", "&lt;");
    out.replace(">", "&gt;");
    out.replace("\"", "&quot;");
    out.replace("'", "&apos;");
    return out;
}

QString paragraphForEntry(const QString& entry)
{
    const QStringList lines = entry.split('\n');
    QString runs;
    for (int i = 0; i < lines.size(); ++i) {
        if (i > 0)
            runs += "<w:r><w:br/></w:r>";
        runs += "<w:r><w:t xml:space=\"preserve\">" + escapeXml(lines[i]) + "</w:t></w:r>";
    }
    return "<w:p>" + runs + "</w:p>";
}

QString buildDocumentXml(const QStringList& entries)
{
    QString body;
    for (int i = 0; i < entries.size(); ++i) {
        if (i > 0)
            body += "<w:p/>";
        body += paragraphForEntry(entries[i]);
    }
    return "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
           "<w:document xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">"
           "<w:body>" + body + "</w:body></w:document>";
}

} // namespace

bool DocxExporter::write(const QString& filePath, const QStringList& entries)
{
    QList<ZipEntry> files;

    ZipEntry contentTypes;
    contentTypes.name = "[Content_Types].xml";
    contentTypes.data =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
        "<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>"
        "<Default Extension=\"xml\" ContentType=\"application/xml\"/>"
        "<Override PartName=\"/word/document.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml\"/>"
        "</Types>";
    files.append(contentTypes);

    ZipEntry rels;
    rels.name = "_rels/.rels";
    rels.data =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
        "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"word/document.xml\"/>"
        "</Relationships>";
    files.append(rels);

    ZipEntry document;
    document.name = "word/document.xml";
    document.data = buildDocumentXml(entries).toUtf8();
    files.append(document);

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write(buildZip(files));
    file.close();
    return true;
}
