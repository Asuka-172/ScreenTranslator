#include "DocumentTranslator.h"
#include "TranslationEngineManager.h"

DocumentTranslator::DocumentTranslator(TranslationEngineManager* manager, QObject* parent)
    : QObject(parent), m_manager(manager)
{
    // 队列连接：避免缓存命中时引擎同步 emit finished 导致重入递归
    connect(m_manager, &TranslationEngineManager::finished, this,
        &DocumentTranslator::onEngineFinished, Qt::QueuedConnection);
    connect(m_manager, &TranslationEngineManager::error, this,
        &DocumentTranslator::onEngineError, Qt::QueuedConnection);
}

QString DocumentTranslator::paragraphText(const DocPara& p) const
{
    QString out;
    for (const DocRun& r : p.runs)
        out += r.text;
    return out.trimmed();
}

void DocumentTranslator::start(const Document& doc, const QString& sourceLang, const QString& targetLang)
{
    m_src = doc;
    m_result = Document();
    m_result.title = doc.title;
    m_sourceLang = sourceLang;
    m_targetLang = targetLang;
    m_cursor = 0;
    m_translatedCount = 0;
    m_total = 0;
    m_cancelled = false;
    m_awaiting = false;

    for (const DocPara& p : doc.paragraphs)
        if (!paragraphText(p).isEmpty())
            ++m_total;

    translateNext();
}

void DocumentTranslator::cancel()
{
    m_cancelled = true;
    m_awaiting = false;
}

void DocumentTranslator::translateNext()
{
    if (m_cancelled)
        return;

    while (m_cursor < m_src.paragraphs.size()) {
        const DocPara& p = m_src.paragraphs[m_cursor];
        const QString text = paragraphText(p);
        if (text.isEmpty()) {
            // 空段落原样保留，不计入翻译进度
            m_result.paragraphs.append(p);
            ++m_cursor;
            continue;
        }

        m_awaiting = true;
        m_manager->translate(text, m_sourceLang, m_targetLang);
        return;
    }

    emit finished(m_result);
}

void DocumentTranslator::onEngineFinished(const QString& text, const QString& lang)
{
    Q_UNUSED(lang);
    if (m_cancelled || !m_awaiting)
        return;
    m_awaiting = false;

    const DocPara& src = m_src.paragraphs[m_cursor];

    // 若源段落整体加粗/斜体，则在译文中保留为段落级样式
    bool allBold = true, allItalic = true, any = false;
    for (const DocRun& r : src.runs) {
        if (r.text.trimmed().isEmpty())
            continue;
        any = true;
        if (!r.bold) allBold = false;
        if (!r.italic) allItalic = false;
    }
    if (!any)
        allBold = allItalic = false;

    DocPara out;
    out.headingLevel = src.headingLevel;
    DocRun r;
    r.text = text;
    r.bold = allBold;
    r.italic = allItalic;
    out.runs.append(r);
    m_result.paragraphs.append(out);

    ++m_cursor;
    ++m_translatedCount;
    emit progress(m_translatedCount, m_total);

    translateNext();
}

void DocumentTranslator::onEngineError(const QString& message)
{
    if (m_cancelled || !m_awaiting)
        return;
    m_awaiting = false;
    emit error(message);
}
