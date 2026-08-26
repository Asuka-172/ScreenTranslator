#include "DocumentExporter.h"
#include "DocxExporter.h"

#include <QFile>
#include <QFont>
#include <QMarginsF>
#include <QPageLayout>
#include <QPageSize>
#include <QPdfWriter>
#include <QTextDocument>

namespace {

QString paraText(const DocPara& p)
{
    QString out;
    for (const DocRun& r : p.runs)
        out += r.text;
    return out;
}

QString escapeHtml(const QString& s)
{
    QString out = s;
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

bool writeTxt(const Document& doc, const QString& path, QString* err)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (err) *err = QStringLiteral("无法写入文件：%1").arg(path);
        return false;
    }

    QStringList blocks;
    for (const DocPara& p : doc.paragraphs) {
        QString text = paraText(p);
        if (p.headingLevel >= 1 && p.headingLevel <= 6) {
            QString marker(p.headingLevel, QLatin1Char('#'));
            marker += QLatin1Char(' ');
            text = marker + text;
        }
        blocks << text;
    }

    file.write(blocks.join(QStringLiteral("\n\n")).toUtf8());
    file.close();
    return true;
}

bool writePdf(const Document& doc, const QString& path, QString* err)
{
    QPdfWriter writer(path);
    writer.setResolution(96);
    writer.setPageSize(QPageSize(QPageSize::A4));
    writer.setPageMargins(QMarginsF(15, 15, 15, 15), QPageLayout::Millimeter);

    QString body;
    for (const DocPara& p : doc.paragraphs)
        body += paraHtml(p);

    QTextDocument td;
    td.setDefaultFont(QFont(QStringLiteral("Microsoft YaHei"), 11));
    td.setHtml(QStringLiteral("<html><body>%1</body></html>").arg(body));
    td.print(&writer);

    if (err) *err = QString();
    return true;
}

} // namespace

bool DocumentExporter::write(const Document& doc, const QString& filePath, Format fmt, QString* err)
{
    switch (fmt) {
    case Format::Txt:
        return writeTxt(doc, filePath, err);
    case Format::Docx:
        return DocxExporter::writeDocument(filePath, doc);
    case Format::Pdf:
        return writePdf(doc, filePath, err);
    }
    if (err) *err = QStringLiteral("不支持的导出格式");
    return false;
}
