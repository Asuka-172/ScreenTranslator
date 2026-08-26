#include "DocumentImporter.h"
#include "ZipReader.h"

#include <QFile>
#include <QFileInfo>
#include <QXmlStreamReader>

#include <poppler-document.h>
#include <poppler-page.h>
#include <poppler-global.h>

#include <algorithm>
#include <cctype>
#include <memory>
#include <vector>

namespace {

bool isCjk(QChar ch)
{
    const ushort u = ch.unicode();
    return (u >= 0x4E00 && u <= 0x9FFF)
        || (u >= 0x3400 && u <= 0x4DBF)
        || (u >= 0xF900 && u <= 0xFAFF);
}

// 判断两个片段拼接时是否需要空格（两侧都是 CJK 或任一侧是 CJK 时不加）
bool needSpace(const QString& a, const QString& b)
{
    if (a.isEmpty() || b.isEmpty())
        return false;
    const QChar last = a.at(a.size() - 1);
    const QChar first = b.at(0);
    if (isCjk(last) || isCjk(first))
        return false;
    return true;
}

QString decodeText(const QByteArray& bytes)
{
    QString u = QString::fromUtf8(bytes);
    if (u.contains(QChar::ReplacementCharacter) && !bytes.isEmpty())
        return QString::fromLocal8Bit(bytes);
    return u;
}

Document importTxt(const QString& path, QString* err)
{
    Document doc;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (err) *err = QStringLiteral("无法打开文件：%1").arg(path);
        return doc;
    }
    const QByteArray bytes = f.readAll();
    f.close();

    doc.title = QFileInfo(path).completeBaseName();
    QString content = decodeText(bytes);
    content.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    content.replace(QChar('\r'), QChar('\n'));

    const QStringList lines = content.split(QLatin1Char('\n'));
    for (const QString& raw : lines) {
        const QString line = raw.trimmed();
        if (line.isEmpty())
            continue;
        DocPara p;
        DocRun r;
        r.text = line;
        p.runs.append(r);
        doc.paragraphs.append(p);
    }
    return doc;
}

int headingFromStyle(const QString& styleId)
{
    const QString s = styleId.toLower();
    if (s.startsWith(QLatin1String("heading")) || s.startsWith(QStringLiteral("标题"))) {
        for (int i = 1; i <= 6; ++i)
            if (s.endsWith(QString::number(i)))
                return i;
        return 1;
    }
    if (s == QLatin1String("title") || s == QStringLiteral("标题"))
        return 1;
    return 0;
}

Document importDocx(const QString& path, QString* err)
{
    Document doc;
    ZipReader zip;
    if (!zip.load(path, err))
        return doc;

    bool ok = false;
    const QByteArray xml = zip.readEntry(QStringLiteral("word/document.xml"), &ok);
    if (!ok || xml.isEmpty()) {
        if (err) *err = QStringLiteral("未在文档中找到 word/document.xml");
        return doc;
    }

    doc.title = QFileInfo(path).completeBaseName();

    QXmlStreamReader reader(xml);
    reader.setNamespaceProcessing(true);

    DocPara para;
    DocRun run;
    bool inPara = false, inRun = false, inRPr = false, inPPr = false;
    bool rBold = false, rItalic = false, rUnderline = false;
    QString pStyle;
    int outlineLvl = -1;

    auto flushRun = [&]() {
        if (inRun && !run.text.isEmpty()) {
            run.bold = rBold;
            run.italic = rItalic;
            run.underline = rUnderline;
            para.runs.append(run);
        }
        run = DocRun();
        rBold = rItalic = rUnderline = false;
        inRun = false;
    };

    auto flushPara = [&]() {
        flushRun();
        if (inPara) {
            int hl = headingFromStyle(pStyle);
            if (hl == 0 && outlineLvl >= 0 && outlineLvl < 9)
                hl = outlineLvl + 1;
            para.headingLevel = hl;
            if (!para.runs.isEmpty())
                doc.paragraphs.append(para);
        }
        para = DocPara();
        pStyle.clear();
        outlineLvl = -1;
        inPara = false;
    };

    while (!reader.atEnd()) {
        switch (reader.readNext()) {
        case QXmlStreamReader::StartElement: {
            const QString n = reader.name().toString();
            if (n == QLatin1String("p")) { flushPara(); inPara = true; }
            else if (n == QLatin1String("pPr")) { inPPr = true; }
            else if (n == QLatin1String("pStyle") && inPPr) { pStyle = reader.attributes().value(QLatin1String("w:val")).toString(); }
            else if (n == QLatin1String("outlineLvl") && inPPr) { outlineLvl = reader.attributes().value(QLatin1String("w:val")).toString().toInt(); }
            else if (n == QLatin1String("r")) { flushRun(); inRun = true; }
            else if (n == QLatin1String("rPr") && inRun) { inRPr = true; }
            else if (n == QLatin1String("b") && inRPr) { rBold = true; }
            else if (n == QLatin1String("i") && inRPr) { rItalic = true; }
            else if (n == QLatin1String("u") && inRPr) { rUnderline = true; }
            else if (n == QLatin1String("t") && inRun) { run.text += reader.readElementText(); }
            else if (n == QLatin1String("tab") && inRun) { run.text += QChar('\t'); }
            else if (n == QLatin1String("br") && inRun) { run.text += QChar('\n'); }
            break;
        }
        case QXmlStreamReader::EndElement: {
            const QString n = reader.name().toString();
            if (n == QLatin1String("r")) flushRun();
            else if (n == QLatin1String("rPr")) inRPr = false;
            else if (n == QLatin1String("pPr")) inPPr = false;
            else if (n == QLatin1String("p")) flushPara();
            break;
        }
        default:
            break;
        }
    }

    if (reader.hasError()) {
        if (err) *err = QStringLiteral("解析 docx 失败：%1").arg(reader.errorString());
        return Document();
    }
    flushPara();
    return doc;
}

struct PdfBox {
    QString text;
    double top;
    double height;
    double size;
    bool bold;
    bool italic;
    int page;
};

struct PdfLine {
    QString text;
    double top;
    double height;
    double size;
    bool bold;
    bool italic;
    int page;
};

bool fontHas(const std::string& font, const char* needle)
{
    return font.find(needle) != std::string::npos;
}

Document importPdf(const QString& path, QString* err)
{
    Document doc;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (err) *err = QStringLiteral("无法打开文件：%1").arg(path);
        return doc;
    }
    const QByteArray bytes = f.readAll();
    f.close();

    std::unique_ptr<poppler::document> pd(
        poppler::document::load_from_raw_data(bytes.constData(), int(bytes.size())));
    if (!pd || pd->is_locked()) {
        if (err) *err = QStringLiteral("无法解析 PDF（可能已加密或损坏）：%1").arg(path);
        return doc;
    }

    doc.title = QFileInfo(path).completeBaseName();

    std::vector<PdfBox> boxes;
    const int pageCount = pd->pages();
    for (int i = 0; i < pageCount; ++i) {
        std::unique_ptr<poppler::page> pg(pd->create_page(i));
        if (!pg)
            continue;
        const std::vector<poppler::text_box> tbs = pg->text_list();
        for (const poppler::text_box& tb : tbs) {
            const auto utf8 = tb.text().to_utf8(); // byte_array = std::vector<char>
            if (utf8.empty())
                continue;
            const std::string font = tb.get_font_name();
            std::string low = font;
            std::transform(low.begin(), low.end(), low.begin(),
                [](unsigned char c) { return char(std::tolower(c)); });

            PdfBox b;
            b.text = QString::fromUtf8(utf8.data(), int(utf8.size()));
            b.top = tb.bbox().top();
            b.height = tb.bbox().height();
            b.size = tb.get_font_size();
            b.bold = fontHas(low, "bold") || fontHas(low, "black");
            b.italic = fontHas(low, "italic") || fontHas(low, "oblique");
            b.page = i;
            boxes.push_back(b);
        }
    }

    // 按阅读顺序（页面内已排序）把 box 归并为行
    std::vector<PdfLine> lines;
    PdfLine curLine;
    bool haveLine = false;
    int curPage = -1;
    for (const PdfBox& b : boxes) {
        const bool pageChanged = (b.page != curPage);
        if (pageChanged) {
            if (haveLine)
                lines.push_back(curLine);
            haveLine = false;
            curPage = b.page;
        }

        if (!haveLine) {
            curLine = PdfLine{ b.text, b.top, b.height, b.size, b.bold, b.italic, b.page };
            haveLine = true;
        } else if (std::abs(b.top - curLine.top) > 0.5 * curLine.height) {
            lines.push_back(curLine);
            curLine = PdfLine{ b.text, b.top, b.height, b.size, b.bold, b.italic, b.page };
        } else {
            if (needSpace(curLine.text, b.text))
                curLine.text += QLatin1Char(' ');
            curLine.text += b.text;
            curLine.height = std::max(curLine.height, b.height);
            curLine.size = std::max(curLine.size, b.size);
            curLine.bold = curLine.bold || b.bold;
            curLine.italic = curLine.italic || b.italic;
        }
    }
    if (haveLine)
        lines.push_back(curLine);

    // 正文中位字号（用于判定标题）
    std::vector<double> sizes;
    sizes.reserve(lines.size());
    for (const PdfLine& ln : lines)
        if (ln.size > 0)
            sizes.push_back(ln.size);
    std::sort(sizes.begin(), sizes.end());
    const double median = sizes.empty() ? 0.0 : sizes[sizes.size() / 2];

    auto headingLevel = [&](double s) -> int {
        if (median <= 0 || s <= 0)
            return 0;
        const double r = s / median;
        if (r >= 1.6) return 1;
        if (r >= 1.4) return 2;
        if (r >= 1.2) return 3;
        return 0;
    };

    QList<DocPara> paras;
    DocPara cur;
    bool haveCur = false;
    double prevTop = 0, prevHeight = 0;
    int paraPage = -1;

    auto flushCur = [&]() {
        if (haveCur)
            paras.push_back(cur);
        cur = DocPara();
        haveCur = false;
    };

    for (const PdfLine& ln : lines) {
        const int hl = headingLevel(ln.size);
        if (hl > 0) {
            flushCur();
            DocPara hp;
            hp.headingLevel = hl;
            DocRun hr;
            hr.text = ln.text;
            hr.bold = ln.bold;
            hr.italic = ln.italic;
            hp.runs.append(hr);
            paras.push_back(hp);
            prevTop = ln.top;
            prevHeight = ln.height;
            continue;
        }

        if (!haveCur) {
            DocRun r;
            r.text = ln.text;
            r.bold = ln.bold;
            r.italic = ln.italic;
            cur.runs.append(r);
            haveCur = true;
            paraPage = ln.page;
        } else {
            const double gap = ln.top - (prevTop + prevHeight);
            const bool newPara = (ln.page != paraPage) || (gap > 1.2 * prevHeight);
            if (newPara) {
                flushCur();
                DocRun r;
                r.text = ln.text;
                r.bold = ln.bold;
                r.italic = ln.italic;
                cur.runs.append(r);
                haveCur = true;
                paraPage = ln.page;
            } else {
                DocRun& r = cur.runs[0];
                if (needSpace(r.text, ln.text))
                    r.text += QLatin1Char(' ');
                r.text += ln.text;
                r.bold = r.bold || ln.bold;
                r.italic = r.italic || ln.italic;
            }
        }
        prevTop = ln.top;
        prevHeight = ln.height;
    }
    flushCur();

    doc.paragraphs = paras;
    return doc;
}

} // namespace

Document DocumentImporter::import(const QString& path, QString* err)
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix == QLatin1String("txt"))
        return importTxt(path, err);
    if (suffix == QLatin1String("docx"))
        return importDocx(path, err);
    if (suffix == QLatin1String("pdf"))
        return importPdf(path, err);

    if (err) *err = QStringLiteral("不支持的文件格式：.%1").arg(suffix);
    return Document();
}
