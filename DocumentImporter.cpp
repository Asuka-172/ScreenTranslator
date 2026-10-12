#include "DocumentImporter.h"
#include "ZipReader.h"

#include <QFile>
#include <QFileInfo>
#include <QXmlStreamReader>
#include <QVector>

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

// ---------- 公式检测与拆分 ----------
// 强数学字符：只出现在数学/符号区，正常正文几乎不含，可作公式锚点。
bool isStrongMath(uint cp)
{
    return (cp >= 0x1D400 && cp <= 0x1D7FF)      // 数学字母数字符号（如 𝑡 𝑛 𝑇）
        || (cp >= 0x2200 && cp <= 0x22FF)        // 数学运算符（∈∀∃∑…）
        || (cp >= 0x2190 && cp <= 0x21FF)        // 箭头
        || cp == 0x221A || cp == 0x221E || cp == 0x222B || cp == 0x2202
        || cp == 0x2207 || cp == 0x00D7 || cp == 0x00B1;
}

// 可并入公式 span 的“邻接”字符（把 t_n、x+y、α=β 整体包进公式 run）
bool isMathAdjacent(uint cp)
{
    if ((cp >= 'A' && cp <= 'Z') || (cp >= 'a' && cp <= 'z')
        || (cp >= '0' && cp <= '9'))
        return true;
    if (cp >= 0x0370 && cp <= 0x03FF) return true;      // 希腊字母
    if (cp >= 0x2080 && cp <= 0x2089) return true;      // 下标数字
    if (cp >= 0x2070 && cp <= 0x2079) return true;      // 上标数字
    switch (cp) {
    case '(': case ')': case '[': case ']': case '{': case '}':
    case '+': case '-': case '=': case '*': case '/': case '^': case '_':
    case '|': case 0x00B7: case ',': case '.': case ';': case ':':
    case 0x2212: case 0x2217: case 0x22C5: case 0x2264: case 0x2265:
    case 0x2260: case 0x2248: case 0x2192: case 0x21D2:
    case 0x2208: case 0x2209: case 0x2229: case 0x222A:
        return true;
    default:
        return false;
    }
}

// 该行是否为“公式主导”（≥25% 强数学字符）：用于标题判定时排除公式行。
bool isFormulaDominant(const QString& text)
{
    int strong = 0, total = 0;
    for (int i = 0; i < text.size(); ++i) {
        uint cp = text.at(i).unicode();
        if (QChar::isHighSurrogate(cp) && i + 1 < text.size()) {
            const uint lo = text.at(i + 1).unicode();
            if (QChar::isLowSurrogate(lo)) {
                cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                ++i;
            }
        }
        if (cp == ' ' || cp == '\t') continue;
        ++total;
        if (isStrongMath(cp)) ++strong;
    }
    return total > 0 && strong * 4 >= total;
}

// 标记 URL/DOI 片段为公式（不送翻译，避免被引擎拆成单字符/插入零宽空格）
void markUrlSpans(const QString& t, QVector<bool>& m)
{
    const auto isUrlChar = [](QChar c) {
        if (c.isSpace())
            return false;
        switch (c.unicode()) {
        case '<': case '>': case ')': case ']': case '，': case '。': case '；': case ';':
        case 0x3002: case 0xFF09: case 0xFF0C: case 0xFF1B:
            return false;
        default:
            return true;
        }
    };
    const QString low = t.toLower();
    const int n = t.size();
    for (int i = 0; i + 4 < n; ++i) {
        int start = -1;
        if (low.mid(i, 8) == QLatin1String("https://")) start = i;
        else if (low.mid(i, 7) == QLatin1String("http://")) start = i;
        else if (low.mid(i, 4) == QLatin1String("www.")) start = i;
        else if (low.mid(i, 4) == QLatin1String("doi.")) start = i;
        else if (low.mid(i, 4) == QLatin1String("doi:")) start = i;
        if (start < 0)
            continue;
        int j = start;
        while (j < n && isUrlChar(t.at(j))) {
            m[j] = true;
            ++j;
        }
        i = j - 1;
    }
}

// 折叠字间距形式的 URL："h t t p s : / / d o i . o r g / 1 0 ..." → "https://doi.org/10..."
// 源 PDF 有时把 URL 的每个字符渲染成独立文本框（字间距），需先还原为连续串。
QString collapseLetterSpacedUrls(const QString& text)
{
    const int n = text.size();
    if (n < 8)
        return text;
    QString out;
    out.reserve(n);
    int i = 0;
    while (i < n) {
        bool urlStart = false;
        if (i + 7 <= n) {
            const QString seg = text.mid(i, 7).toLower();
            urlStart = (seg == QLatin1String("h t t p")
                        || seg == QLatin1String("d o i .")
                        || seg == QLatin1String("w w w ."));
        }
        if (urlStart) {
            int j = i;
            while (j < n) {
                out += text.at(j);
                if (j + 2 < n && text.at(j + 1) == QLatin1Char(' ')
                    && (j + 3 >= n || text.at(j + 3) == QLatin1Char(' '))) {
                    j += 2;
                    continue;
                }
                ++j;
                break;
            }
            i = j;
        } else {
            out += text.at(i);
            ++i;
        }
    }
    return out;
}

// 把段落内嵌的公式片段拆成 formula run（原样保留），其余为普通文本 run。
void splitFormulaRuns(DocPara& p)
{
    QList<DocRun> out;
    for (const DocRun& r : p.runs) {
        if (r.formula) {
            out.append(r);
            continue;
        }
        const QString t = collapseLetterSpacedUrls(r.text);
        if (t.isEmpty()) {
            out.append(r);
            continue;
        }

        QVector<bool> m(t.size(), false);
        for (int i = 0; i < t.size(); ++i) {
            uint cp = t.at(i).unicode();
            if (QChar::isHighSurrogate(cp) && i + 1 < t.size()) {
                const uint lo = t.at(i + 1).unicode();
                if (QChar::isLowSurrogate(lo)) {
                    const uint full = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                    if (isStrongMath(full)) {
                        m[i] = true;
                        m[i + 1] = true;
                    }
                    ++i;
                    continue;
                }
            }
            if (isStrongMath(cp)) m[i] = true;
        }

        // 向两侧扩展，把公式 token 整体纳入
        bool changed = true;
        while (changed) {
            changed = false;
            for (int i = 0; i < t.size(); ++i) {
                if (m[i]) continue;
                const bool prevMath = (i > 0 && m[i - 1]);
                const bool nextMath = (i + 1 < t.size() && m[i + 1]);
                const QChar c = t.at(i);
                if ((prevMath || nextMath) && !c.isSpace() && isMathAdjacent(c.unicode())) {
                    m[i] = true;
                    changed = true;
                }
            }
        }

        // 标记 URL/DOI，原样保留
        markUrlSpans(t, m);

        int i = 0;
        while (i < t.size()) {
            int j = i;
            while (j < t.size() && m[j] == m[i]) ++j;
            DocRun nr;
            nr.text = t.mid(i, j - i);
            nr.bold = r.bold;
            nr.italic = r.italic;
            nr.underline = r.underline;
            nr.formula = m[i];
            out.append(nr);
            i = j;
        }
    }

    // 合并相邻同属性 run，去掉空 run
    QList<DocRun> merged;
    for (const DocRun& r : out) {
        if (r.text.isEmpty())
            continue;
        if (!merged.isEmpty()
            && merged.last().formula == r.formula
            && merged.last().bold == r.bold
            && merged.last().italic == r.italic
            && merged.last().underline == r.underline) {
            merged.last().text += r.text;
        } else {
            merged.append(r);
        }
    }
    p.runs = merged;
}

// 编号标题：如 "1. Introduction" / "1.1 Dimensions" / "2.1.1. Effects" → 层级 = 段数（≤6）。
// 编号行须简短（标题短、列表项是长句，用长度区分）。
int numberedHeadingLevel(const QString& text)
{
    const QString t = text.trimmed();
    const int n = t.size();
    if (n == 0 || n > 90)
        return 0;
    int i = 0;
    while (i < n && t.at(i).isDigit()) ++i;
    if (i == 0 || i >= n) return 0;          // 不以数字开头，或纯数字（页码）
    if (t.at(i) != QLatin1Char('.')) return 0; // 数字后必须跟点号
    int parts = 1;
    int j = i + 1;
    while (true) {
        int k = j;
        while (k < n && (t.at(k) == QLatin1Char(' ') || t.at(k) == QLatin1Char('\t'))) ++k;
        if (k >= n) return 0;                // "1. " 结尾无内容
        if (!t.at(k).isDigit()) break;       // "1. Introduction"
        int m = k;
        while (m < n && t.at(m).isDigit()) ++m;
        if (m < n && t.at(m) == QLatin1Char('.')) { ++parts; j = m + 1; continue; }
        if (m < n && t.at(m).isSpace()) { ++parts; break; }  // "1.1 Introduction"
        return 0;
    }
    return qMin(parts, 6);
}

// 目录行：含 " . ." 点线（应排除在标题判定外）
bool hasDotLeader(const QString& text)
{
    return text.contains(QStringLiteral(" . .")) || text.contains(QStringLiteral(". . ."));
}

// 清理点线：从首个 " . ." 起截断（连同原 PDF 页码一并去掉）
QString cleanDotLeaders(const QString& text)
{
    const int idx = text.indexOf(QStringLiteral(" . ."));
    if (idx < 0)
        return text.trimmed();
    return text.left(idx).trimmed();
}

// 页码行：纯 1-4 位数字（无字母/标点）
bool isPageNumberLine(const QString& text)
{
    const QString t = text.trimmed();
    if (t.isEmpty() || t.size() > 4)
        return false;
    for (const QChar c : t)
        if (!c.isDigit())
            return false;
    return true;
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
            b.size = tb.get_font_size() > 0 ? tb.get_font_size() : tb.bbox().height();
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

    // 正文中位字号（用于判定标题）。排除公式主导行，避免抬高基准。
    std::vector<double> sizes;
    sizes.reserve(lines.size());
    for (const PdfLine& ln : lines)
        if (ln.size > 0 && !isFormulaDominant(ln.text))
            sizes.push_back(ln.size);
    std::sort(sizes.begin(), sizes.end());
    const double median = sizes.empty() ? 0.0 : sizes[sizes.size() / 2];

    bool inReferences = false;   // 进入参考文献区后，编号行不再判为标题

    auto headingLevel = [&](const PdfLine& ln) -> int {
        // 公式行（如独立编号的等式）字号可能较大，但绝非标题
        if (isFormulaDominant(ln.text))
            return 0;
        // 目录行（点线）不是标题
        if (hasDotLeader(ln.text))
            return 0;

        const QString t = ln.text.trimmed();
        // "References / 参考文献 / Bibliography" 本身是标题，并开启参考文献区
        if (t.compare(QStringLiteral("references"), Qt::CaseInsensitive) == 0
            || t.compare(QStringLiteral("reference"), Qt::CaseInsensitive) == 0
            || t.compare(QStringLiteral("bibliography"), Qt::CaseInsensitive) == 0
            || t == QStringLiteral("参考文献")) {
            inReferences = true;
            return 1;
        }
        // 编号标题：1. / 1.1 / 1.2.3 → 层级按段数；参考文献区内的编号条目不是标题
        const int numLevel = numberedHeadingLevel(t);
        if (numLevel > 0)
            return inReferences ? 0 : numLevel;
        // 字号（bbox 高度）回退判定
        const double s = ln.size;
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
    bool curFormulaOnly = false;   // 当前段落是否仅含公式行
    double prevTop = 0, prevHeight = 0;
    int paraPage = -1;

    auto flushCur = [&]() {
        if (haveCur)
            paras.push_back(cur);
        cur = DocPara();
        haveCur = false;
        curFormulaOnly = false;
    };

    for (const PdfLine& ln : lines) {
        // 页码行（纯数字）跳过
        if (isPageNumberLine(ln.text)) {
            prevTop = ln.top;
            prevHeight = ln.height;
            continue;
        }

        // 目录行（点线）：清理点线后各自独立成段，避免互相合并
        if (hasDotLeader(ln.text)) {
            flushCur();
            DocPara tp;
            DocRun tr;
            tr.text = cleanDotLeaders(ln.text);
            tr.bold = ln.bold;
            tr.italic = ln.italic;
            tp.runs.append(tr);
            paras.push_back(tp);
            prevTop = ln.top;
            prevHeight = ln.height;
            continue;
        }

        const QString lineText = ln.text;

        const int hl = headingLevel(ln);
        if (hl > 0) {
            flushCur();
            DocPara hp;
            hp.headingLevel = hl;
            DocRun hr;
            hr.text = lineText;
            hr.bold = ln.bold;
            hr.italic = ln.italic;
            hp.runs.append(hr);
            paras.push_back(hp);
            prevTop = ln.top;
            prevHeight = ln.height;
            continue;
        }

        // 公式主导行：独立成段（相邻公式行合并为一段），避免被并入正文段落
        const bool formulaLine = isFormulaDominant(lineText);
        if (haveCur && curFormulaOnly != formulaLine)
            flushCur();

        if (!haveCur) {
            DocRun r;
            r.text = lineText;
            r.bold = ln.bold;
            r.italic = ln.italic;
            cur.runs.append(r);
            haveCur = true;
            curFormulaOnly = formulaLine;
            paraPage = ln.page;
        } else if (curFormulaOnly && formulaLine) {
            DocRun& r = cur.runs[0];
            if (needSpace(r.text, lineText))
                r.text += QLatin1Char(' ');
            r.text += lineText;
            r.bold = r.bold || ln.bold;
            r.italic = r.italic || ln.italic;
        } else {
            const double gap = ln.top - (prevTop + prevHeight);
            const bool newPara = (ln.page != paraPage) || (gap > 1.2 * prevHeight);
            if (newPara) {
                flushCur();
                DocRun r;
                r.text = lineText;
                r.bold = ln.bold;
                r.italic = ln.italic;
                cur.runs.append(r);
                haveCur = true;
                curFormulaOnly = false;
                paraPage = ln.page;
            } else {
                DocRun& r = cur.runs[0];
                if (needSpace(r.text, lineText))
                    r.text += QLatin1Char(' ');
                r.text += lineText;
                r.bold = r.bold || ln.bold;
                r.italic = r.italic || ln.italic;
            }
        }
        prevTop = ln.top;
        prevHeight = ln.height;
    }
    flushCur();

    // 拆分公式 run：公式/符号原样保留，其余文本才参与翻译
    for (DocPara& p : paras)
        splitFormulaRuns(p);

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
