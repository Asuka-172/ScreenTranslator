#include "DocOutputSession.h"
#include "DocxExporter.h"

#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QMarginsF>
#include <QPageLayout>
#include <QPageSize>
#include <QPdfWriter>
#include <QTextDocument>

namespace {

QString stripControlChars(const QString& s)
{
    QString out;
    out.reserve(s.size());
    for (const QChar c : s) {
        const ushort u = c.unicode();
        if (u < 0x20 && u != 0x09 && u != 0x0A && u != 0x0D)
            continue;
        out.append(c);
    }
    return out;
}

QString paraPlainText(const DocPara& p)
{
    QString out;
    for (const DocRun& r : p.runs)
        out += r.text;
    out = stripControlChars(out);
    if (p.headingLevel >= 1 && p.headingLevel <= 6) {
        QString marker(p.headingLevel, QLatin1Char('#'));
        marker += QLatin1Char(' ');
        out = marker + out;
    }
    return out;
}

QString escapeHtml(const QString& s)
{
    QString out = stripControlChars(s);
    out.replace(QLatin1Char('&'), QStringLiteral("&amp;"));
    out.replace(QLatin1Char('<'), QStringLiteral("&lt;"));
    out.replace(QLatin1Char('>'), QStringLiteral("&gt;"));
    out.replace(QLatin1Char('\n'), QStringLiteral("<br/>"));
    return out;
}

QString paraHtml(const DocPara& p)
{
    QString inner;
    for (const DocRun& r : p.runs) {
        QString t = escapeHtml(r.text);
        if (r.bold)
            t = QStringLiteral("<b>%1</b>").arg(t);
        if (r.italic)
            t = QStringLiteral("<i>%1</i>").arg(t);
        if (r.underline)
            t = QStringLiteral("<u>%1</u>").arg(t);
        inner += t;
    }

    if (inner.isEmpty())
        return QStringLiteral("<p>&nbsp;</p>");
    if (p.headingLevel >= 1 && p.headingLevel <= 6) {
        const QString tag = QStringLiteral("h%1").arg(p.headingLevel);
        return QStringLiteral("<%1>%2</%1>").arg(tag, inner);
    }
    return QStringLiteral("<p>%1</p>").arg(inner);
}

} // namespace

DocOutputSession::DocOutputSession(const QString& filePath, Format fmt)
    : m_filePath(filePath), m_fmt(fmt)
{
}

DocOutputSession::~DocOutputSession()
{
    abort();
}

DocOutputSession::Format DocOutputSession::formatFromSuffix(const QString& filePath)
{
    const QString suf = QFileInfo(filePath).suffix().toLower();
    if (suf == QLatin1String("pdf"))
        return Format::Pdf;
    if (suf == QLatin1String("docx"))
        return Format::Docx;
    return Format::Txt;
}

bool DocOutputSession::open(const Document& seed, QString* err)
{
    m_acc = seed;
    m_closed = false;

    switch (m_fmt) {
    case Format::Txt: {
        m_txtFile = new QFile(m_filePath);
        if (!m_txtFile->open(QIODevice::WriteOnly | QIODevice::Text)) {
            if (err) *err = QStringLiteral("无法写入文件：%1").arg(m_filePath);
            abort();
            return false;
        }
        writeTxtAll();
        break;
    }
    case Format::Docx: {
        if (!DocxExporter::writeDocument(m_filePath, m_acc)) {
            if (err) *err = QStringLiteral("无法写入文件：%1").arg(m_filePath);
            abort();
            return false;
        }
        break;
    }
    case Format::Pdf: {
        m_pdfWriter = new QPdfWriter(m_filePath);
        m_pdfWriter->setResolution(96);
        m_pdfWriter->setPageSize(QPageSize(QPageSize::A4));
        m_pdfWriter->setPageMargins(QMarginsF(15, 15, 15, 15), QPageLayout::Millimeter);
        printParasToPdf(m_acc.paragraphs);
        break;
    }
    }
    return true;
}

bool DocOutputSession::append(const DocPara& para, QString* err)
{
    Q_UNUSED(err);
    m_acc.paragraphs.append(para);
    switch (m_fmt) {
    case Format::Txt:
        // 不立即落盘，由 flush() 按批次重写（避免频繁 I/O）
        break;
    case Format::Docx:
        // 同 txt：周期性 flush 重写
        break;
    case Format::Pdf:
        printParaToPdf(para);
        break;
    }
    return true;
}

bool DocOutputSession::flush(QString* err)
{
    switch (m_fmt) {
    case Format::Txt:
        if (m_txtFile) {
            writeTxtAll();
            m_txtFile->flush();
        }
        break;
    case Format::Docx:
        if (!DocxExporter::writeDocument(m_filePath, m_acc)) {
            if (err) *err = QStringLiteral("无法写入文件：%1").arg(m_filePath);
            return false;
        }
        break;
    case Format::Pdf:
        break;
    }
    return true;
}

bool DocOutputSession::finish(QString* err)
{
    const bool ok = flush(err);
    switch (m_fmt) {
    case Format::Txt:
        if (m_txtFile) {
            m_txtFile->close();
            delete m_txtFile;
            m_txtFile = nullptr;
        }
        break;
    case Format::Docx:
        break;
    case Format::Pdf:
        // QPdfWriter 在析构时收尾写入；销毁前无需 end()
        if (m_pdfWriter) {
            delete m_pdfWriter;
            m_pdfWriter = nullptr;
        }
        break;
    }
    m_closed = true;
    return ok;
}

void DocOutputSession::abort()
{
    if (m_closed)
        return;
    switch (m_fmt) {
    case Format::Txt:
        if (m_txtFile) {
            m_txtFile->flush();
            m_txtFile->close();
            delete m_txtFile;
            m_txtFile = nullptr;
        }
        break;
    case Format::Docx:
        break;
    case Format::Pdf:
        if (m_pdfWriter) {
            delete m_pdfWriter;
            m_pdfWriter = nullptr;
        }
        break;
    }
    m_closed = true;
}

void DocOutputSession::writeTxtAll()
{
    if (!m_txtFile)
        return;
    m_txtFile->resize(0);
    QStringList blocks;
    blocks.reserve(m_acc.paragraphs.size());
    for (const DocPara& p : m_acc.paragraphs)
        blocks << paraPlainText(p);
    m_txtFile->write(blocks.join(QStringLiteral("\n\n")).toUtf8());
}

void DocOutputSession::printParaToPdf(const DocPara& para)
{
    if (!m_pdfWriter)
        return;
    QTextDocument td;
    td.setDefaultFont(QFont(QStringLiteral("Microsoft YaHei"), 11));
    td.setHtml(QStringLiteral("<html><body>%1</body></html>").arg(paraHtml(para)));
    td.print(m_pdfWriter);
}

void DocOutputSession::printParasToPdf(const QList<DocPara>& paras)
{
    if (!m_pdfWriter)
        return;
    if (paras.isEmpty())
        return;
    QTextDocument td;
    td.setDefaultFont(QFont(QStringLiteral("Microsoft YaHei"), 11));
    QString body;
    for (const DocPara& p : paras)
        body += paraHtml(p);
    td.setHtml(QStringLiteral("<html><body>%1</body></html>").arg(body));
    td.print(m_pdfWriter);
}
