#include "OfflineTranslator.h"
#include "AppConfig.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QMetaObject>
#include <algorithm>

OfflineTranslator::OfflineTranslator(QObject* parent)
    : ITranslator(parent)
{
}

QString OfflineTranslator::name() const
{
    return QStringLiteral("离线翻译（本地词典）");
}

QString OfflineTranslator::defaultModelDir()
{
    return QCoreApplication::applicationDirPath() + QStringLiteral("/models");
}

QString OfflineTranslator::defaultGlossaryPath()
{
    return defaultModelDir() + QStringLiteral("/glossary.txt");
}

QString OfflineTranslator::resolvedModelDir() const
{
    const QString p = AppConfig::instance().offlineModelPath().trimmed();
    return p.isEmpty() ? defaultModelDir() : p;
}

QString OfflineTranslator::resolvedGlossaryPath() const
{
    const QString p = AppConfig::instance().offlineGlossaryPath().trimmed();
    return p.isEmpty() ? defaultGlossaryPath() : p;
}

void OfflineTranslator::translate(const QString& text,
    const QString& sourceLang,
    const QString& targetLang)
{
    Q_UNUSED(sourceLang);
    Q_UNUSED(targetLang);

    if (!AppConfig::instance().offlineEnabled()) {
        QMetaObject::invokeMethod(this, [this]() {
            emit error(QStringLiteral("离线翻译未启用，请在设置中开启"));
        }, Qt::QueuedConnection);
        return;
    }

    loadGlossary();

    QString result = text;
    for (const auto& entry : m_entries)
        result.replace(entry.first, entry.second, Qt::CaseSensitive);

    QMetaObject::invokeMethod(this, [this, result]() {
        emit finished(result, QString());
    }, Qt::QueuedConnection);
}

void OfflineTranslator::reloadGlossary()
{
    m_glossaryMtime = QDateTime();
    loadGlossary();
}

void OfflineTranslator::loadGlossary()
{
    const QString path = resolvedGlossaryPath();
    QFileInfo info(path);
    if (info.exists() && info.lastModified() == m_glossaryMtime)
        return; // 未变化，无需重载

    m_entries.clear();
    m_glossaryMtime = info.exists() ? info.lastModified() : QDateTime();

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    QTextStream in(&file);
    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith('#'))
            continue;

        QString src, dst;
        const int tab = line.indexOf('\t');
        const int eq = line.indexOf('=');
        if (tab >= 0) {
            src = line.left(tab).trimmed();
            dst = line.mid(tab + 1).trimmed();
        } else if (eq >= 0) {
            src = line.left(eq).trimmed();
            dst = line.mid(eq + 1).trimmed();
        } else {
            continue;
        }

        if (!src.isEmpty() && !dst.isEmpty())
            m_entries.append(qMakePair(src, dst));
    }

    // 长词优先，避免短词先行替换破坏长短语
    std::sort(m_entries.begin(), m_entries.end(),
        [](const QPair<QString, QString>& a, const QPair<QString, QString>& b) {
            return a.first.size() > b.first.size();
        });
}

QStringList OfflineTranslator::listModelFiles() const
{
    QDir dir(resolvedModelDir());
    if (!dir.exists())
        return {};
    return dir.entryList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
}
