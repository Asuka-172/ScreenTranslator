#include "DocumentTranslator.h"
#include "TranslationEngineManager.h"
#include "AppConfig.h"
#include "DocOutputSession.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

namespace {

QJsonObject runToJson(const DocRun& r)
{
    QJsonObject o;
    o[QStringLiteral("t")] = r.text;
    o[QStringLiteral("b")] = r.bold;
    o[QStringLiteral("i")] = r.italic;
    o[QStringLiteral("u")] = r.underline;
    o[QStringLiteral("f")] = r.formula;
    return o;
}

DocRun runFromJson(const QJsonObject& o)
{
    DocRun r;
    r.text = o[QStringLiteral("t")].toString();
    r.bold = o[QStringLiteral("b")].toBool();
    r.italic = o[QStringLiteral("i")].toBool();
    r.underline = o[QStringLiteral("u")].toBool();
    r.formula = o[QStringLiteral("f")].toBool();
    return r;
}

QJsonObject paraToJson(const DocPara& p)
{
    QJsonObject o;
    o[QStringLiteral("h")] = p.headingLevel;
    QJsonArray runs;
    for (const DocRun& r : p.runs)
        runs.append(runToJson(r));
    o[QStringLiteral("runs")] = runs;
    return o;
}

DocPara paraFromJson(const QJsonObject& o)
{
    DocPara p;
    p.headingLevel = o[QStringLiteral("h")].toInt();
    const QJsonArray runs = o[QStringLiteral("runs")].toArray();
    for (const QJsonValue& v : runs)
        p.runs.append(runFromJson(v.toObject()));
    return p;
}

QJsonArray docParasToJson(const QList<DocPara>& paras)
{
    QJsonArray arr;
    for (const DocPara& p : paras)
        arr.append(paraToJson(p));
    return arr;
}

QList<DocPara> docParasFromJson(const QJsonArray& arr)
{
    QList<DocPara> out;
    for (const QJsonValue& v : arr)
        out.append(paraFromJson(v.toObject()));
    return out;
}

} // namespace

DocumentTranslator::DocumentTranslator(TranslationEngineManager* manager, QObject* parent)
    : QObject(parent), m_manager(manager)
{
    // 队列连接：引擎在缓存命中时可能同步 emit finished，排队可避免重入递归
    connect(m_manager, &TranslationEngineManager::finished, this,
        &DocumentTranslator::onEngineFinished, Qt::QueuedConnection);
    connect(m_manager, &TranslationEngineManager::error, this,
        &DocumentTranslator::onEngineError, Qt::QueuedConnection);
}

DocumentTranslator::~DocumentTranslator()
{
    delete m_output;
}

QString DocumentTranslator::paragraphText(const DocPara& p) const
{
    QString out;
    for (const DocRun& r : p.runs)
        out += r.text;
    return out;
}

bool DocumentTranslator::isParaEmpty(const DocPara& p) const
{
    return paragraphText(p).trimmed().isEmpty();
}

// 按字节上限切分长文本：优先在句末标点/空白后切，其次硬切（不拆开 UTF-16 代理对）。
QStringList DocumentTranslator::chunkText(const QString& text, int maxBytes)
{
    QStringList out;
    if (text.isEmpty() || maxBytes <= 0)
        return out;
    if (text.toUtf8().size() <= maxBytes) {
        out << text;
        return out;
    }

    const auto isSentenceEnd = [](QChar c) {
        switch (c.unicode()) {
        case 0x3002: case 0xFF01: case 0xFF1F: case 0xFF1B:   // 。！？；
        case '.': case '!': case '?': case ';': case '\n':
            return true;
        default:
            return false;
        }
    };

    QString cur;
    int goodCut = -1;
    const int n = text.size();
    int i = 0;
    while (i < n) {
        const QChar c = text.at(i);
        if (!cur.isEmpty()) {
            const int curBytes = cur.toUtf8().size();
            const int addBytes = QString(c).toUtf8().size();
            if (curBytes + addBytes > maxBytes) {
                int cut = goodCut > 0 ? goodCut : -1;
                if (cut <= 0) {
                    // 硬切：逐字符找不超过上限的最长前缀
                    cut = cur.size();
                    while (cut > 0 && cur.left(cut).toUtf8().size() > maxBytes)
                        --cut;
                    if (cut > 0 && cut < cur.size()) {
                        const QChar hi = cur.at(cut - 1);
                        const QChar lo = cur.at(cut);
                        if (hi.isHighSurrogate() && lo.isLowSurrogate())
                            --cut;
                    }
                }
                if (cut <= 0)
                    cut = 1;
                out << cur.left(cut);
                cur = cur.mid(cut);
                goodCut = -1;
                continue;   // 重新评估剩余文本
            }
        }
        cur.append(c);
        if (isSentenceEnd(c) || c.isSpace())
            goodCut = cur.size();
        ++i;
    }
    if (!cur.isEmpty())
        out << cur;
    return out;
}

void DocumentTranslator::start(const Document& doc, const QString& sourceLang,
    const QString& targetLang, const QString& inputPath, const QString& outputPath,
    int resumeNextPara, const QList<DocPara>& seedTranslated)
{
    m_src = doc;
    m_sourceLang = sourceLang;
    m_targetLang = targetLang;
    m_inputPath = inputPath;
    m_outputPath = outputPath;
    m_cancelled = false;
    m_failed = false;
    m_nextPara = 0;
    m_nextFeed = 0;
    m_inFlight = 0;
    m_doneNonEmpty = 0;
    m_totalNonEmpty = 0;
    m_paraPending.clear();
    m_segResults.clear();
    m_tokenToSeg.clear();
    m_paraSegRange.clear();
    m_segments.clear();

    m_result = Document();
    m_result.title = doc.title;
    m_result.paragraphs = seedTranslated;

    m_maxInFlight = qBound(1, AppConfig::instance().docConcurrency(), 8);

    // 输出会话：续传时以已提交段落为种子（同一文件接续写入）
    delete m_output;
    m_output = new DocOutputSession(outputPath, DocOutputSession::formatFromSuffix(outputPath));
    Document seed;
    seed.title = doc.title;
    seed.paragraphs = seedTranslated;
    QString err;
    if (!m_output->open(seed, &err)) {
        delete m_output;
        m_output = nullptr;
        m_failed = true;
        emit error(err);
        return;
    }

    for (const DocPara& p : m_src.paragraphs)
        if (!isParaEmpty(p))
            ++m_totalNonEmpty;
    for (const DocPara& p : seedTranslated)
        if (!isParaEmpty(p))
            ++m_doneNonEmpty;

    buildSegments();

    // 续传：跳过已译段落（m_nextPara 从 resumeNextPara 开始；其前段不再投递）
    m_nextPara = resumeNextPara;
    m_nextFeed = 0;
    while (m_nextFeed < m_segments.size()
           && m_segments[m_nextFeed].paraIndex < resumeNextPara)
        ++m_nextFeed;

    feedWorkers();
    // 立即尝试提交：空段落无需翻译即可推进；若全文为空则直接完成。
    // （非空段落的提交由引擎结果回调驱动）
    tryCommit();
}

void DocumentTranslator::cancel()
{
    if (m_cancelled || m_failed)
        return;
    m_cancelled = true;
    for (auto it = m_tokenToSeg.constBegin(); it != m_tokenToSeg.constEnd(); ++it)
        m_manager->cancelRequest(it.key());
    m_tokenToSeg.clear();
    m_paraPending.clear();
    if (m_output) {
        QString err;
        m_output->flush(&err);
        m_output->finish(&err);
        delete m_output;
        m_output = nullptr;
    }
    saveCheckpoint();
}

int DocumentTranslator::doneCount() const
{
    return m_doneNonEmpty;
}

void DocumentTranslator::buildSegments()
{
    const int maxBytes = qMax(500, AppConfig::instance().docChunkMaxBytes());
    m_paraSegRange.reserve(m_src.paragraphs.size());

    for (int pi = 0; pi < m_src.paragraphs.size(); ++pi) {
        const DocPara& p = m_src.paragraphs[pi];
        if (isParaEmpty(p)) {
            m_paraSegRange.append(qMakePair(-1, -1));
            continue;
        }

        // 按「相邻且样式三元组相同、公式属性相同」的 run 归组
        struct Group { bool bold = false, italic = false, underline = false, formula = false; QString text; };
        QList<Group> groups;
        for (const DocRun& r : p.runs) {
            if (r.text.trimmed().isEmpty())
                continue;
            if (!groups.isEmpty()
                && groups.last().bold == r.bold
                && groups.last().italic == r.italic
                && groups.last().underline == r.underline
                && groups.last().formula == r.formula) {
                groups.last().text += r.text;
            } else {
                Group g;
                g.bold = r.bold;
                g.italic = r.italic;
                g.underline = r.underline;
                g.formula = r.formula;
                g.text = r.text;
                groups.append(g);
            }
        }

        if (groups.isEmpty()) {
            m_paraSegRange.append(qMakePair(-1, -1));
            continue;
        }

        const int first = m_segments.size();
        for (int gi = 0; gi < groups.size(); ++gi) {
            const Group& g = groups[gi];
            // 公式组：不切块、不翻译，整组作为一个 segment 原样保留
            const QStringList chunks = g.formula
                ? QStringList{ g.text }
                : chunkText(g.text.trimmed(), maxBytes);
            for (int ci = 0; ci < chunks.size(); ++ci) {
                Segment s;
                s.paraIndex = pi;
                s.groupIndex = gi;
                s.chunkIndex = ci;
                s.groupChunks = chunks.size();
                s.text = chunks[ci];
                s.bold = g.bold;
                s.italic = g.italic;
                s.underline = g.underline;
                s.formula = g.formula;
                m_segments.append(s);
            }
        }
        m_paraSegRange.append(qMakePair(first, m_segments.size() - 1));
        // 该段尚未完成的分段数：初始化为总分段数，每完成一个分段递减。
        // 这样 tryCommit 不会把“尚未投递”的段落误判为已完成（否则串行时会整段提交为空）。
        m_paraPending[pi] = m_segments.size() - first;
    }
}

void DocumentTranslator::feedWorkers()
{
    if (m_cancelled || m_failed)
        return;
    while (m_inFlight < m_maxInFlight && m_nextFeed < m_segments.size()) {
        const Segment& seg = m_segments[m_nextFeed];
        // 公式段：原样保留，直接视为“已就绪”，不送引擎
        if (seg.formula) {
            m_segResults.insert(m_nextFeed, seg.text);
            --m_paraPending[seg.paraIndex];
            ++m_nextFeed;
            continue;
        }
        const quint64 token = m_manager->translateAsync(seg.text, m_sourceLang, m_targetLang);
        m_tokenToSeg.insert(token, m_nextFeed);
        // 文本段：pending 已在 buildSegments 中按总分段数初始化，此处不再 ++；
        // 结果返回时在 onEngineFinished 中 --。
        ++m_inFlight;
        ++m_nextFeed;
    }
}

void DocumentTranslator::assemblePara(int paraIndex, DocPara& out) const
{
    out = DocPara();
    const DocPara& src = m_src.paragraphs[paraIndex];
    out.headingLevel = src.headingLevel;
    if (isParaEmpty(src))
        return;

    const QPair<int, int> range = m_paraSegRange[paraIndex];
    if (range.first < 0)
        return;

    int lastGroup = -1;
    DocRun cur;
    for (int s = range.first; s <= range.second; ++s) {
        const Segment& seg = m_segments[s];
        const QString t = m_segResults.value(s);
        if (seg.groupIndex != lastGroup) {
            if (lastGroup >= 0)
                out.runs.append(cur);
            cur = DocRun();
            cur.bold = seg.bold;
            cur.italic = seg.italic;
            cur.underline = seg.underline;
            lastGroup = seg.groupIndex;
        }
        cur.text += t;
    }
    if (lastGroup >= 0)
        out.runs.append(cur);
}

void DocumentTranslator::tryCommit()
{
    if (m_cancelled || m_failed)
        return;

    while (m_nextPara < m_src.paragraphs.size()) {
        const int pending = m_paraPending.value(m_nextPara, 0);
        if (pending > 0)
            break;

        DocPara out;
        assemblePara(m_nextPara, out);
        m_result.paragraphs.append(out);
        if (m_output)
            m_output->append(out);
        if (!isParaEmpty(m_src.paragraphs[m_nextPara]))
            ++m_doneNonEmpty;
        ++m_nextPara;

        emit progress(m_doneNonEmpty, m_totalNonEmpty,
            QStringLiteral("段 %1/%2 · 在途 %3")
                .arg(m_nextPara).arg(m_src.paragraphs.size()).arg(m_inFlight));

        // 周期性把半成品落盘（txt/docx 重写；pdf 已增量打印）
        if (m_output && (m_doneNonEmpty % 20) == 0) {
            QString err;
            m_output->flush(&err);
        }

        saveCheckpoint();
    }

    if (m_nextPara >= m_src.paragraphs.size()) {
        // 全部完成
        m_cancelled = true;
        QString err;
        if (m_output) {
            m_output->finish(&err);
            delete m_output;
            m_output = nullptr;
        }
        removeCheckpoint(m_inputPath);
        emit finished(m_result);
        return;
    }

    feedWorkers();
}

void DocumentTranslator::onEngineFinished(const QString& text, const QString& lang, quint64 requestId)
{
    Q_UNUSED(lang);
    if (m_cancelled || m_failed)
        return;
    const int segIndex = m_tokenToSeg.value(requestId, -1);
    if (segIndex < 0 || segIndex >= m_segments.size())
        return;
    m_tokenToSeg.remove(requestId);
    --m_inFlight;
    m_segResults.insert(segIndex, text);
    --m_paraPending[m_segments[segIndex].paraIndex];
    tryCommit();
}

void DocumentTranslator::onEngineError(const QString& message, quint64 requestId)
{
    if (m_cancelled || m_failed)
        return;
    const int segIndex = m_tokenToSeg.value(requestId, -1);
    m_tokenToSeg.remove(requestId);
    --m_inFlight;
    int para = 0;
    if (segIndex >= 0 && segIndex < m_segments.size()) {
        para = m_segments[segIndex].paraIndex;
        m_segResults.remove(segIndex);
    }
    failAndStop(QStringLiteral("第 %1 段翻译失败：%2（已保存进度，可继续）")
                    .arg(para + 1).arg(message));
}

void DocumentTranslator::failAndStop(const QString& message)
{
    if (m_failed)
        return;
    m_failed = true;
    for (auto it = m_tokenToSeg.constBegin(); it != m_tokenToSeg.constEnd(); ++it)
        m_manager->cancelRequest(it.key());
    m_tokenToSeg.clear();
    m_paraPending.clear();

    if (m_output) {
        QString err;
        m_output->flush(&err);
        m_output->finish(&err);
        delete m_output;
        m_output = nullptr;
    }
    saveCheckpoint();
    emit error(message);
}

// ---------- 断点续传 ----------

QString DocumentTranslator::resumeDir()
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (dir.isEmpty())
        dir = QStringLiteral(".");
    return dir + QStringLiteral("/resume");
}

QString DocumentTranslator::resumePathFor(const QString& inputPath)
{
    const QString key = QFileInfo(inputPath).absoluteFilePath();
    const QByteArray hash = QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Md5).toHex();
    return resumeDir() + QLatin1Char('/') + QString::fromLatin1(hash) + QStringLiteral(".json");
}

bool DocumentTranslator::hasResume(const QString& inputPath)
{
    return QFileInfo::exists(resumePathFor(inputPath));
}

bool DocumentTranslator::loadCheckpoint(const QString& inputPath, ResumeData& out)
{
    const QString path = resumePathFor(inputPath);
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;
    const QByteArray data = f.readAll();
    f.close();

    QJsonParseError perr;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &perr);
    if (perr.error != QJsonParseError::NoError || !doc.isObject())
        return false;
    const QJsonObject root = doc.object();
    if (root.value(QStringLiteral("version")).toInt() != 2)
        return false;

    out.source.title = root.value(QStringLiteral("title")).toString();
    out.source.paragraphs = docParasFromJson(root.value(QStringLiteral("paragraphs")).toArray());
    out.translated = docParasFromJson(root.value(QStringLiteral("translated")).toArray());
    out.srcLang = root.value(QStringLiteral("sourceLang")).toString();
    out.tgtLang = root.value(QStringLiteral("targetLang")).toString();
    out.engineId = root.value(QStringLiteral("engineId")).toString();
    out.inputPath = root.value(QStringLiteral("inputPath")).toString(inputPath);
    out.outputPath = root.value(QStringLiteral("outputPath")).toString();
    out.nextPara = root.value(QStringLiteral("nextPara")).toInt();
    out.doneNonEmpty = root.value(QStringLiteral("doneNonEmpty")).toInt();
    out.totalNonEmpty = root.value(QStringLiteral("totalNonEmpty")).toInt();
    out.chunkMaxBytes = root.value(QStringLiteral("chunkMaxBytes")).toInt();
    return true;
}

void DocumentTranslator::removeCheckpoint(const QString& inputPath)
{
    if (inputPath.isEmpty())
        return;
    QFile::remove(resumePathFor(inputPath));
}

void DocumentTranslator::saveCheckpoint()
{
    if (m_inputPath.isEmpty() || !AppConfig::instance().resumeEnabled())
        return;

    QDir().mkpath(resumeDir());

    QJsonObject root;
    root[QStringLiteral("version")] = 2;
    root[QStringLiteral("inputPath")] = m_inputPath;
    root[QStringLiteral("outputPath")] = m_outputPath;
    root[QStringLiteral("sourceLang")] = m_sourceLang;
    root[QStringLiteral("targetLang")] = m_targetLang;
    root[QStringLiteral("engineId")] = m_manager->currentEngine();
    root[QStringLiteral("chunkMaxBytes")] = AppConfig::instance().docChunkMaxBytes();
    root[QStringLiteral("nextPara")] = m_nextPara;
    root[QStringLiteral("doneNonEmpty")] = m_doneNonEmpty;
    root[QStringLiteral("totalNonEmpty")] = m_totalNonEmpty;
    root[QStringLiteral("title")] = m_src.title;
    root[QStringLiteral("paragraphs")] = docParasToJson(m_src.paragraphs);
    root[QStringLiteral("translated")] = docParasToJson(m_result.paragraphs);
    root[QStringLiteral("updatedAt")] = QDateTime::currentDateTime().toString(Qt::ISODate);

    QFile f(resumePathFor(m_inputPath));
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        f.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
        f.close();
    }
}
